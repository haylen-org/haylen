#include "haylen/assets/Manager.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

#include "graphics/ShaderResource.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/JsonValidator.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::assets {

struct Manager::Publisher {
    explicit Publisher(core::EventBus& eventBus) : events(&eventBus) {}

    void publish(std::string_view name, const std::string& type, const std::string& path) {
        const std::scoped_lock lock(mutex);
        if (events != nullptr) {
            events->post(std::string(name), {{"type", type}, {"path", path}});
        }
    }

    std::mutex mutex;
    core::EventBus* events;
};

std::string Manager::lowercase(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
}

std::string Manager::describe(const core::Json& value) {
    return value.dump(-1, ' ', false, core::Json::error_handler_t::replace);
}

text::TrueTypeFont::Options Manager::fontOptionsFromJson(const core::Json& options) {
    core::JsonValidator::requireKnownOptions(options, {"bakeSize", "spread", "atlasSize"}, "font options");
    // clang-format off
    const auto number = [&options](const char* key, auto value) {
        if (!options.contains(key)) {
            return value;
        }
        if (!options.at(key).is_number()) {
            throw std::invalid_argument(std::string("The font option \"") + key + "\" must be a number, not " + describe(options.at(key)) + ".");
        }
        return options.at(key).get<decltype(value)>();
    };
    // clang-format on

    text::TrueTypeFont::Options result;
    result.bakeSize = number("bakeSize", result.bakeSize);
    result.spread = number("spread", result.spread);
    result.atlasSize = number("atlasSize", result.atlasSize);
    return result;
}

core::Json Manager::fontOptionsToJson(const text::TrueTypeFont::Options& options) {
    return {{"bakeSize", core::JsonNumber::fromFloat(options.bakeSize)}, {"spread", options.spread}, {"atlasSize", options.atlasSize}};
}

graphics::Texture::Options Manager::textureOptionsFromJson(const core::Json& options) {
    core::JsonValidator::requireKnownOptions(options, {"filter", "wrap", "mipmaps"}, "texture options");
    graphics::Texture::Options result;
    if (options.contains("filter")) {
        const core::Json& value = options.at("filter");
        const std::optional<graphics::Texture::Filter> filter = value.is_string() ? graphics::Texture::filterFromName(value.get<std::string>()) : std::nullopt;
        if (!filter) {
            throw std::invalid_argument("The texture filter must be \"nearest\" or \"linear\", not " + describe(value) + ".");
        }
        result.filter = *filter;
    }
    if (options.contains("wrap")) {
        const core::Json& value = options.at("wrap");
        const std::optional<graphics::Texture::Wrap> wrap = value.is_string() ? graphics::Texture::wrapFromName(value.get<std::string>()) : std::nullopt;
        if (!wrap) {
            throw std::invalid_argument("The texture wrap must be \"clamp\", \"repeat\" or \"mirror\", not " + describe(value) + ".");
        }
        result.wrap = *wrap;
    }
    if (options.contains("mipmaps")) {
        const core::Json& value = options.at("mipmaps");
        if (!value.is_boolean()) {
            throw std::invalid_argument("The texture option \"mipmaps\" must be true or false, not " + describe(value) + ".");
        }
        result.mipmaps = value.get<bool>();
    }
    return result;
}

core::Json Manager::textureOptionsToJson(graphics::Texture::Options options) {
    return {{"filter", graphics::Texture::filterName(options.filter)}, {"wrap", graphics::Texture::wrapName(options.wrap)}, {"mipmaps", options.mipmaps}};
}

Manager::Manager(io::Package& contentPackage, core::JobSystem& jobSystem, graphics::Device& graphicsDevice, core::EventBus& eventBus) : package(contentPackage), jobs(jobSystem), device(graphicsDevice), publisher(std::make_shared<Publisher>(eventBus)) {
    // clang-format off
    registerType({
        .name = "texture",
        .extensions = {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".gif"},
        .normalize = [](const core::Json& options) { return textureOptionsToJson(textureOptionsFromJson(options)); },
        .decode = [](Request& request) -> std::shared_ptr<void> {
            return std::make_shared<graphics::Image>(graphics::Image::decodeFor(request.bytes, textureOptionsFromJson(request.options)));
        },
        .finalize = [this](std::shared_ptr<void> decoded, const Request& request) -> std::shared_ptr<void> {
            const graphics::Image& image = *std::static_pointer_cast<graphics::Image>(decoded);
            const std::shared_ptr<graphics::TextureResource> texture = device.createTexture(image, textureOptionsFromJson(request.options)).getResource();
            texture->label = request.path;
            return texture;
        },
        .reload = [this](const std::shared_ptr<void>& asset, Request& request) {
            device.replaceTexture(graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(asset)), graphics::Image::decodeFor(request.bytes, textureOptionsFromJson(request.options)));
        },
    });
    // An image file read into pixels on the CPU, decoded on the worker pool, which the app composes and packs before it becomes a texture.
    registerType({
        .name = "image",
        .extensions = {},
        .normalize = [](const core::Json& options) {
            core::JsonValidator::requireKnownOptions(options, {}, "image options");
            return core::Json::object();
        },
        .decode = [](Request& request) -> std::shared_ptr<void> {
            return std::make_shared<graphics::Image>(graphics::Image::decode(request.bytes));
        },
        .finalize = [](std::shared_ptr<void> decoded, const Request&) { return decoded; },
    });
    registerType({
        .name = "font",
        .extensions = {".ttf", ".otf"},
        .normalize = [](const core::Json& options) { return fontOptionsToJson(fontOptionsFromJson(options)); },
        .decode = [](Request& request) -> std::shared_ptr<void> {
            return std::make_shared<std::vector<std::uint8_t>>(std::move(request.bytes));
        },
        .finalize = [this](std::shared_ptr<void> decoded, const Request& request) -> std::shared_ptr<void> {
            auto& data = *std::static_pointer_cast<std::vector<std::uint8_t>>(decoded);
            const std::shared_ptr<text::Font> font = std::make_shared<text::TrueTypeFont>(device, std::move(data), fontOptionsFromJson(request.options));
            return font;
        },
    });
    registerType({
        .name = "shader",
        .extensions = {".shader"},
        .normalize = [](const core::Json& options) {
            core::JsonValidator::requireKnownOptions(options, {}, "shader asset options");
            return core::Json::object();
        },
        .decode = [this](Request& request) -> std::shared_ptr<void> {
            const graphics::Shader shader = graphics::Shader::parse(request.bytes);
            shader.getResource()->prepare(device.getGpu(), jobs);
            return shader.getResource();
        },
        .finalize = [](std::shared_ptr<void> decoded, const Request&) { return decoded; },
        .reload = [this](const std::shared_ptr<void>& asset, Request& request) {
            const graphics::Shader fresh = graphics::Shader::parse(request.bytes);
            fresh.getResource()->prepare(device.getGpu(), jobs);
            graphics::Shader(std::static_pointer_cast<graphics::ShaderResource>(asset)).replace(fresh);
        },
    });
    registerType({
        .name = "vectorImage",
        .extensions = {".svg"},
        .normalize = [](const core::Json& options) {
            core::JsonValidator::requireKnownOptions(options, {}, "vector image asset options");
            return core::Json::object();
        },
        .decode = [](Request& request) -> std::shared_ptr<void> {
            return graphics::VectorImage::parse(request.bytes).getResource();
        },
        .finalize = [](std::shared_ptr<void> decoded, const Request&) { return decoded; },
    });
    registerType({
        .name = "json",
        .extensions = {".json"},
        .normalize = [](const core::Json& options) {
            core::JsonValidator::requireKnownOptions(options, {}, "JSON asset options");
            return core::Json::object();
        },
        .decode = [](Request& request) -> std::shared_ptr<void> {
            return std::make_shared<core::Json>(core::Json::parse(request.bytes.begin(), request.bytes.end()));
        },
        .finalize = [](std::shared_ptr<void> decoded, const Request&) { return decoded; },
        .reload = [](const std::shared_ptr<void>& asset, Request& request) {
            *std::static_pointer_cast<core::Json>(asset) = core::Json::parse(request.bytes.begin(), request.bytes.end());
        },
    });
    // clang-format on
}

Manager::~Manager() {
    const std::scoped_lock lock(publisher->mutex);
    publisher->events = nullptr;
}

void Manager::registerType(Type type) {
    if (type.name.empty() || !type.normalize || !type.decode || !type.finalize) {
        throw std::invalid_argument("An asset type needs a name, an option normalizer, a decoder and a finalizer.");
    }
    for (std::string& extension : type.extensions) {
        extension = lowercase(extension);
    }
    std::string name = type.name;
    types.insert_or_assign(std::move(name), std::move(type));
}

bool Manager::hasType(std::string_view name) const noexcept {
    return types.contains(std::string(name));
}

std::string Manager::getTypeForPath(std::string_view path) const {
    const std::string extension = lowercase(io::Path::extension(path));
    for (const auto& [name, type] : types) {
        if (std::find(type.extensions.begin(), type.extensions.end(), extension) != type.extensions.end()) {
            return name;
        }
    }
    throw std::invalid_argument("No asset type handles the file \"" + std::string(path) + "\". Pass its type or use an extension that an asset type handles.");
}

const Manager::Type& Manager::getType(std::string_view name) const {
    const auto found = types.find(std::string(name));
    if (found == types.end()) {
        throw std::invalid_argument("The asset type \"" + std::string(name) + "\" does not exist.");
    }
    return found->second;
}

std::size_t Manager::reload(std::string_view path) {
    const std::string changed = io::Path::normalize(path);

    // A type may load other assets while it reloads one, so the entries of the file are taken before any of them reloads.
    std::vector<std::pair<std::string, Cached>> entries;
    for (const auto& [key, entry] : cache) {
        if (entry.path == changed) {
            entries.emplace_back(key, entry);
        }
    }

    std::size_t reloaded = 0;
    for (const auto& [key, entry] : entries) {
        const std::shared_ptr<void> asset = entry.asset.lock();
        if (!asset) {
            cache.erase(key);
            continue;
        }
        ++reloaded;
        const Type& kind = getType(entry.type);
        if (!kind.reload) {
            publisher->publish(core::LifecycleEvent::kAssetChanged, entry.type, changed);
            cache.erase(key);
            continue;
        }
        Request fresh{.type = entry.type, .path = changed, .options = entry.options, .package = &package};
        readFile(fresh, isStreamed(fresh, kind));
        kind.reload(asset, fresh);
        publisher->publish(core::LifecycleEvent::kAssetReloaded, entry.type, changed);
    }
    return reloaded;
}

std::string Manager::cacheKey(std::string_view type, std::string_view path, const core::Json& options) {
    return std::string(type) + "|" + std::string(path) + "|" + options.dump();
}

Manager::Request Manager::makeRequest(std::string_view typeName, std::string_view path, const core::Json& options) const {
    return {.type = std::string(typeName), .path = io::Path::normalize(path), .options = normalizeOptions(typeName, options), .package = &package};
}

std::shared_ptr<void> Manager::findCached(const std::string& key) const {
    const auto found = cache.find(key);
    return found == cache.end() ? nullptr : found->second.asset.lock();
}

core::Json Manager::normalizeOptions(std::string_view typeName, const core::Json& options) const {
    return getType(typeName).normalize(options);
}

std::shared_ptr<void> Manager::load(std::string_view typeName, std::string_view path, const core::Json& options) {
    const Type& assetType = getType(typeName);
    // clang-format off
    return share(typeName, path, options, [this, &assetType](const Request& shared) {
        requireLocal(shared.path);
        Request loading = shared;
        readFile(loading, isStreamed(loading, assetType));
        std::shared_ptr<void> decoded = assetType.decode(loading);

        // The dependencies stay cached while the finalizer takes them.
        std::vector<std::shared_ptr<void>> held;
        if (assetType.dependencies) {
            for (const Dependency& dependency : assetType.dependencies(decoded, loading)) {
                held.push_back(load(dependency.type, dependency.path, dependency.options));
            }
        }
        return assetType.finalize(std::move(decoded), loading);
    });
    // clang-format on
}

std::shared_ptr<void> Manager::share(std::string_view typeName, std::string_view path, const core::Json& options, const std::function<std::shared_ptr<void>(const Request&)>& make) {
    const Request request = makeRequest(typeName, path, options);
    const std::string key = cacheKey(request.type, request.path, request.options);
    if (std::shared_ptr<void> cached = findCached(key)) {
        return cached;
    }

    std::shared_ptr<void> asset = track(make(request), request);
    cache.insert_or_assign(key, Cached{.asset = asset, .type = request.type, .path = request.path, .options = request.options});
    return asset;
}

std::shared_ptr<void> Manager::track(std::shared_ptr<void> asset, const Request& request) const {
    publisher->publish(core::LifecycleEvent::kAssetLoaded, request.type, request.path);

    // The deleter of the returned pointer holds the asset, so it runs when the last holder lets go, on whatever thread that is.
    // clang-format off
    return {asset.get(), [inner = asset, weak = std::weak_ptr<Publisher>(publisher), type = request.type, path = request.path](void*) mutable {
        inner.reset();
        if (const std::shared_ptr<Publisher> owner = weak.lock()) {
            owner->publish(core::LifecycleEvent::kAssetUnloaded, type, path);
        }
    }};
    // clang-format on
}

void Manager::loadAsync(std::string_view typeName, std::string_view path, Callback callback, const core::Json& options) {
    startLoad(makeRequest(typeName, path, options), std::move(callback));
}

void Manager::startLoad(Request request, Callback callback) {
    const std::string key = cacheKey(request.type, request.path, request.options);

    // Callbacks always run later on the frame thread, so callers never see them run inside the call itself.
    if (std::shared_ptr<void> cached = findCached(key)) {
        jobs.postToFrame([callback = std::move(callback), cached] { callback(cached, {}); });
        return;
    }

    const bool alreadyLoading = pending.contains(key);
    pending[key].callbacks.push_back(std::move(callback));
    if (alreadyLoading) {
        return;
    }

    const Type& assetType = getType(request.type);
    auto loading = std::make_shared<Request>(std::move(request));
    if (isLocal(loading->path)) {
        readAndDecode(loading, key, assetType);
        return;
    }

    // An asset whose bytes are not on the device comes down first, and then loads like any other.
    // clang-format off
    availability.acquire({loading->path}, {}, [this, owner = std::weak_ptr<bool>(alive), loading, key, &assetType](const std::string& error) {
        if (owner.expired()) {
            return;
        }
        if (error.empty()) {
            readAndDecode(loading, key, assetType);
            return;
        }
        jobs.postToFrame([this, owner, key, loading, finalize = assetType.finalize, error] {
            if (!owner.expired()) {
                uploads.push_back({.key = key, .request = loading, .finalize = finalize, .error = error});
            }
        });
    });
    // clang-format on
}

void Manager::readAndDecode(const std::shared_ptr<Request>& loading, const std::string& key, const Type& assetType) {
    // The file is read on the I/O pool, where waiting on the disk blocks no decoding, then decoded on the task pool and queued on the frame thread, where `finalizePending` creates its GPU resources within the upload budget. An asset that streams decodes on the I/O pool as well, since its decoder reads the file.
    // clang-format off
    const auto deliver = [this, owner = std::weak_ptr<bool>(alive), key, loading, &assetType](std::shared_ptr<void> decoded, std::string error) {
        jobs.postToFrame([this, owner, key, loading, &assetType, decoded = std::move(decoded), error = std::move(error)]() mutable {
            if (!owner.expired()) {
                queueUpload({.key = key, .request = loading, .finalize = assetType.finalize, .decoded = std::move(decoded), .error = std::move(error)}, assetType);
            }
        });
    };
    const auto decodeFile = [loading, decode = assetType.decode, deliver] {
        std::shared_ptr<void> decoded;
        std::string error;
        try {
            decoded = decode(*loading);
        } catch (const std::exception& exception) {
            error = exception.what();
        }
        deliver(std::move(decoded), std::move(error));
    };

    jobs.postIo([this, loading, decodeFile, deliver, streamed = isStreamed(*loading, assetType)] {
        try {
            readFile(*loading, streamed);
        } catch (const std::exception& exception) {
            deliver(nullptr, exception.what());
            return;
        }
        if (streamed) {
            decodeFile();
            return;
        }
        jobs.post(decodeFile);
    });
    // clang-format on
}

// The dependencies load like direct loads, whose callbacks run later on the frame thread, and the asset waits for the last of them, failing with the error of the first one that fails.
void Manager::queueUpload(Upload upload, const Type& assetType) {
    std::vector<Dependency> needed;
    if (assetType.dependencies && upload.decoded) {
        try {
            needed = assetType.dependencies(upload.decoded, *upload.request);
        } catch (const std::exception& exception) {
            upload.error = exception.what();
        }
    }
    if (needed.empty()) {
        uploads.push_back(std::move(upload));
        return;
    }

    auto waiting = std::make_shared<Upload>(std::move(upload));
    auto left = std::make_shared<std::size_t>(needed.size());
    // clang-format off
    const auto arrive = [this, owner = std::weak_ptr<bool>(alive), waiting, left](std::shared_ptr<void> asset, std::string error) {
        if (owner.expired()) {
            return;
        }
        if (asset) {
            waiting->held.push_back(std::move(asset));
        } else if (waiting->error.empty()) {
            waiting->error = std::move(error);
        }
        if (--*left == 0) {
            uploads.push_back(std::move(*waiting));
        }
    };
    // clang-format on
    for (const Dependency& dependency : needed) {
        try {
            startLoad(makeRequest(dependency.type, dependency.path, dependency.options), arrive);
        } catch (const std::exception& exception) {
            arrive(nullptr, exception.what());
        }
    }
}

bool Manager::isStreamed(const Request& request, const Type& assetType) {
    return assetType.streams && assetType.streams(request.options);
}

void Manager::readFile(Request& request, bool streamed) const {
    if (streamed) {
        request.reader = package.openAssetReader(request.path);
        return;
    }
    request.bytes = package.readAsset(request.path);
}

void Manager::setAvailability(Availability value) {
    availability = std::move(value);
}

bool Manager::isLocal(std::string_view path) const {
    return !availability.isLocal || availability.isLocal(path);
}

void Manager::requireLocal(std::string_view path) const {
    if (!isLocal(path)) {
        throw std::runtime_error("The asset \"" + std::string(path) + "\" is not on the device yet, since it downloads when the app asks for it. Load it with \"loadAsync\" or a preload group, which bring it first.");
    }
}

void Manager::finish(const std::string& key, const std::shared_ptr<void>& asset, const std::string& error) {
    std::vector<Callback> callbacks;
    if (const auto found = pending.find(key); found != pending.end()) {
        callbacks = std::move(found->second.callbacks);
        pending.erase(found);
    }

    for (const Callback& callback : callbacks) {
        callback(asset, error);
    }
}

graphics::Texture Manager::texture(std::string_view path, graphics::Texture::Options options) {
    return graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(load("texture", path, textureOptionsToJson(options))));
}

void Manager::textureAsync(std::string_view path, std::function<void(graphics::Texture, std::string)> callback, graphics::Texture::Options options) {
    // clang-format off
    loadAsync("texture", path, [callback = std::move(callback)](std::shared_ptr<void> asset, std::string error) {
        callback(graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(asset)), std::move(error));
    }, textureOptionsToJson(options));
    // clang-format on
}

graphics::Shader Manager::shader(std::string_view path) {
    return graphics::Shader(std::static_pointer_cast<graphics::ShaderResource>(load("shader", path)));
}

graphics::VectorImage Manager::vectorImage(std::string_view path) {
    return graphics::VectorImage(std::static_pointer_cast<graphics::VectorImageResource>(load("vectorImage", path)));
}

std::shared_ptr<text::Font> Manager::font(std::string_view path, text::TrueTypeFont::Options options) {
    return std::static_pointer_cast<text::Font>(load("font", path, fontOptionsToJson(options)));
}

core::Json Manager::json(std::string_view path) {
    return *std::static_pointer_cast<core::Json>(load("json", path));
}

std::string Manager::text(std::string_view path) const {
    requireLocal(io::Path::normalize(path));
    return package.readAssetText(path);
}

std::vector<std::uint8_t> Manager::bytes(std::string_view path) const {
    requireLocal(io::Path::normalize(path));
    return package.readAsset(path);
}

std::vector<std::uint8_t> Manager::bytes(std::string_view path, std::uint64_t offset, std::size_t size) const {
    requireLocal(io::Path::normalize(path));
    return package.readAssetRange(path, offset, size);
}

void Manager::bytesAsync(std::string_view path, BytesCallback callback) {
    readAsync(io::Path::normalize(path), std::nullopt, std::move(callback));
}

void Manager::bytesAsync(std::string_view path, std::uint64_t offset, std::size_t size, BytesCallback callback) {
    readAsync(io::Path::normalize(path), std::pair(offset, size), std::move(callback));
}

void Manager::readAsync(std::string path, std::optional<std::pair<std::uint64_t, std::size_t>> range, BytesCallback callback) {
    auto done = std::make_shared<BytesCallback>(std::move(callback));
    // clang-format off
    auto read = [this, owner = std::weak_ptr<bool>(alive), path, range, done] {
        jobs.runIo([&files = package, path, range] { return range ? files.readAssetRange(path, range->first, range->second) : files.readAsset(path); }, [owner, done](core::JobSystem::Result<std::vector<std::uint8_t>> result) {
            if (!owner.expired()) {
                (*done)(result.isOk() ? std::move(*result.value) : std::vector<std::uint8_t>{}, std::move(result.error));
            }
        });
    };
    if (isLocal(path)) {
        read();
        return;
    }

    // Bytes that are not on the device come down first, as for any asynchronous load.
    availability.acquire({path}, {}, [owner = std::weak_ptr<bool>(alive), read, done](const std::string& error) {
        if (owner.expired()) {
            return;
        }
        if (error.empty()) {
            read();
            return;
        }
        (*done)({}, error);
    });
    // clang-format on
}

std::uint64_t Manager::getFileSize(std::string_view path) const {
    return package.getAssetSize(path);
}

bool Manager::exists(std::string_view path) const {
    return package.assetExists(path);
}

std::vector<std::string> Manager::list(std::string_view directory) const {
    return package.listAssets(directory);
}

void Manager::defineGroup(std::string name, std::vector<GroupManifest::Entry> entries) {
    Group& group = groups[std::move(name)];
    group.entries = std::move(entries);
}

void Manager::defineGroups(const core::Json& manifest) {
    for (auto& [name, entries] : GroupManifest::read(manifest)) {
        defineGroup(name, std::move(entries));
    }
}

std::vector<GroupManifest::Entry> Manager::expand(const std::vector<GroupManifest::Entry>& entries) const {
    std::vector<GroupManifest::Entry> expanded;
    for (const GroupManifest::Entry& entry : entries) {
        if (!entry.path.ends_with('/')) {
            expanded.push_back({.path = entry.path, .type = entry.type.empty() ? getTypeForPath(entry.path) : entry.type, .options = entry.options});
            continue;
        }

        // Folder entries include every file that a registered asset type understands.
        for (const std::string& file : package.listAssets(entry.path)) {
            const std::string extension = lowercase(io::Path::extension(file));
            // clang-format off
            const bool known = std::any_of(types.begin(), types.end(), [&extension](const auto& item) {
                return std::find(item.second.extensions.begin(), item.second.extensions.end(), extension) != item.second.extensions.end();
            });
            // clang-format on
            if (known) {
                expanded.push_back({.path = file, .type = entry.type.empty() ? getTypeForPath(file) : entry.type, .options = entry.options});
            }
        }
    }
    return expanded;
}

std::vector<std::string> Manager::getGroupPaths(std::string_view group) const {
    const auto found = groups.find(std::string(group));
    if (found == groups.end()) {
        throw std::invalid_argument("The asset group \"" + std::string(group) + "\" is not defined.");
    }
    std::vector<std::string> paths;
    for (const GroupManifest::Entry& entry : expand(found->second.entries)) {
        paths.push_back(entry.path);
    }
    return paths;
}

void Manager::preload(std::string_view name, GroupProgress progress, GroupCompletion completion) {
    const auto found = groups.find(std::string(name));
    if (found == groups.end()) {
        throw std::invalid_argument("The asset group \"" + std::string(name) + "\" is not defined.");
    }

    Group& group = found->second;
    if (group.loaded) {
        // clang-format off
        jobs.postToFrame([progress = std::move(progress), completion = std::move(completion), errors = group.errors] {
            if (progress) {
                progress(1.0F);
            }
            if (completion) {
                completion(errors);
            }
        });
        // clang-format on
        return;
    }

    // A group that starts to load turns every entry into a request before it changes, so entries that are not valid leave no listener behind and never leave the group loading.
    std::vector<Request> requests;
    if (!group.loading) {
        for (const GroupManifest::Entry& entry : expand(group.entries)) {
            requests.push_back(makeRequest(entry.type, entry.path, entry.options));
        }
    }

    if (progress) {
        group.progressListeners.push_back(std::move(progress));
    }
    if (completion) {
        group.completionListeners.push_back(std::move(completion));
    }
    if (group.loading) {
        return;
    }

    group.held.clear();
    group.errors.clear();
    group.completed = 0;
    group.downloadShare = 0.0F;
    group.downloaded = 0.0F;
    group.total = requests.size();
    group.loading = true;
    const std::uint64_t generation = ++group.generation;
    const std::string groupName(name);

    if (requests.empty()) {
        jobs.postToFrame([this, groupName, generation] { completeGroup(groupName, generation); });
        return;
    }

    // Assets whose bytes are not on the device come down first in one request, which takes half of the progress of the group, and then every asset loads.
    std::vector<std::string> missing;
    for (const Request& request : requests) {
        if (!isLocal(request.path) && std::ranges::find(missing, request.path) == missing.end()) {
            missing.push_back(request.path);
        }
    }
    if (missing.empty()) {
        loadGroup(groupName, generation, std::move(requests));
        return;
    }
    group.downloadShare = 0.5F;
    // clang-format off
    availability.acquire(missing, [this, groupName, generation](float fraction) {
        Group& target = groups.at(groupName);
        if (target.generation == generation) {
            target.downloaded = fraction;
            reportGroupProgress(target);
        }
    }, [this, owner = std::weak_ptr<bool>(alive), groupName, generation, requests = std::move(requests)](const std::string& error) mutable {
        if (owner.expired() || groups.at(groupName).generation != generation) {
            return;
        }
        Group& target = groups.at(groupName);
        target.downloaded = 1.0F;
        if (!error.empty()) {
            std::erase_if(requests, [this, &target, &error](const Request& request) {
                if (isLocal(request.path)) {
                    return false;
                }
                target.errors.push_back(request.path + ": " + error);
                ++target.completed;
                return true;
            });
        }
        if (requests.empty()) {
            completeGroup(groupName, generation);
            return;
        }
        loadGroup(groupName, generation, std::move(requests));
    });
    // clang-format on
}

void Manager::loadGroup(const std::string& groupName, std::uint64_t generation, std::vector<Request> requests) {
    for (Request& request : requests) {
        const std::string path = request.path;
        // clang-format off
        startLoad(std::move(request), [this, groupName, generation, path](std::shared_ptr<void> asset, std::string error) {
            Group& target = groups.at(groupName);
            if (target.generation != generation) {
                return;
            }

            if (asset) {
                target.held.push_back(std::move(asset));
            } else {
                target.errors.push_back(path + ": " + error);
            }
            ++target.completed;
            reportGroupProgress(target);
            if (target.completed == target.total) {
                completeGroup(groupName, generation);
            }
        });
        // clang-format on
    }
}

void Manager::reportGroupProgress(Group& group) const {
    const float loaded = static_cast<float>(group.completed) / static_cast<float>(group.total);
    const float fraction = group.downloadShare * group.downloaded + (1.0F - group.downloadShare) * loaded;
    const std::vector<GroupProgress> listeners = group.progressListeners;
    for (const GroupProgress& listener : listeners) {
        listener(fraction);
    }
}

void Manager::completeGroup(const std::string& name, std::uint64_t generation) {
    Group& group = groups.at(name);
    if (group.generation != generation) {
        return;
    }

    group.loading = false;
    group.loaded = true;
    group.progressListeners.clear();
    const std::vector<GroupCompletion> listeners = std::exchange(group.completionListeners, {});
    const std::vector<std::string> errors = group.errors;
    for (const GroupCompletion& listener : listeners) {
        listener(errors);
    }
}

void Manager::unloadGroup(std::string_view name) {
    const auto found = groups.find(std::string(name));
    if (found == groups.end()) {
        throw std::invalid_argument("The asset group \"" + std::string(name) + "\" is not defined.");
    }

    Group& group = found->second;
    std::vector<GroupCompletion> waiting = std::exchange(group.completionListeners, {});
    group.held.clear();
    group.errors.clear();
    group.progressListeners.clear();
    group.completed = 0;
    group.loading = false;
    group.loaded = false;
    ++group.generation;

    // Preloads that still wait for the group complete on a later frame, with the one failure that the group unloaded first.
    if (!waiting.empty()) {
        // clang-format off
        jobs.postToFrame([waiting = std::move(waiting), errors = std::vector<std::string>{"The asset group \"" + std::string(name) + "\" was unloaded before it finished loading."}] {
            for (const GroupCompletion& listener : waiting) {
                listener(errors);
            }
        });
        // clang-format on
    }
}

float Manager::getGroupProgress(std::string_view name) const {
    const auto found = groups.find(std::string(name));
    if (found == groups.end()) {
        throw std::invalid_argument("The asset group \"" + std::string(name) + "\" is not defined.");
    }
    const Group& group = found->second;
    if (group.loaded) {
        return 1.0F;
    }
    return group.total == 0 ? 0.0F : static_cast<float>(group.completed) / static_cast<float>(group.total);
}

bool Manager::isGroupLoaded(std::string_view name) const {
    const auto found = groups.find(std::string(name));
    return found != groups.end() && found->second.loaded;
}

std::vector<std::string> Manager::getGroups() const {
    std::vector<std::string> names;
    names.reserve(groups.size());
    for (const auto& [name, group] : groups) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::size_t Manager::getCachedCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(cache.begin(), cache.end(), [](const auto& entry) { return !entry.second.asset.expired(); }));
}

std::size_t Manager::getPendingCount() const noexcept {
    return pending.size();
}

std::size_t Manager::releaseUnused() {
    return static_cast<std::size_t>(std::erase_if(cache, [](const auto& entry) { return entry.second.asset.expired(); }));
}

void Manager::finalizePending() {
    const auto start = std::chrono::steady_clock::now();
    const auto budget = std::chrono::duration<double>(uploadBudget);
    do {
        if (uploads.empty()) {
            return;
        }
        Upload upload = std::move(uploads.front());
        uploads.pop_front();

        // A synchronous load may have cached the asset while this one decoded, and every load of an asset shares that one object.
        std::shared_ptr<void> asset = findCached(upload.key);
        if (asset) {
            upload.error.clear();
        } else if (upload.error.empty()) {
            try {
                const Request& request = *upload.request;
                asset = track(upload.finalize(std::move(upload.decoded), request), request);
                cache.insert_or_assign(upload.key, Cached{.asset = asset, .type = request.type, .path = request.path, .options = request.options});
            } catch (const std::exception& exception) {
                upload.error = exception.what();
            }
        }
        finish(upload.key, asset, upload.error);
    } while (std::chrono::steady_clock::now() - start < budget);
}

void Manager::setUploadBudget(double seconds) {
    if (seconds < 0.0) {
        throw std::invalid_argument("The upload budget cannot be negative.");
    }
    uploadBudget = seconds;
}

void Manager::cancelAll() noexcept {
    uploads.clear();
    pending.clear();
    for (auto& [name, group] : groups) {
        group.progressListeners.clear();
        group.completionListeners.clear();
        group.loading = false;
        ++group.generation;
    }
}

} // namespace haylen::assets
