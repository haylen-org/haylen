#include "haylen/assets/Manager.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
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

text::TrueTypeFont::Options Manager::fontOptionsFromJson(const core::Json& options) {
    core::JsonValidator::requireKnownKeys(options, {"bakeSize", "spread", "atlasSize"}, "font options");
    text::TrueTypeFont::Options result;
    result.bakeSize = options.value("bakeSize", result.bakeSize);
    result.spread = options.value("spread", result.spread);
    result.atlasSize = options.value("atlasSize", result.atlasSize);
    return result;
}

core::Json Manager::fontOptionsToJson(const text::TrueTypeFont::Options& options) {
    return {{"bakeSize", core::JsonNumber::fromFloat(options.bakeSize)}, {"spread", options.spread}, {"atlasSize", options.atlasSize}};
}

graphics::Texture::Options Manager::textureOptionsFromJson(const core::Json& options) {
    core::JsonValidator::requireKnownKeys(options, {"filter", "wrap"}, "texture options");
    graphics::Texture::Options result;
    if (options.contains("filter")) {
        const auto filter = graphics::Texture::filterFromName(options.at("filter").get<std::string>());
        if (!filter) {
            throw std::invalid_argument("The texture filter must be \"nearest\" or \"linear\", not \"" + options.at("filter").get<std::string>() + "\".");
        }
        result.filter = *filter;
    }
    if (options.contains("wrap")) {
        const auto wrap = graphics::Texture::wrapFromName(options.at("wrap").get<std::string>());
        if (!wrap) {
            throw std::invalid_argument("The texture wrap must be \"clamp\", \"repeat\" or \"mirror\", not \"" + options.at("wrap").get<std::string>() + "\".");
        }
        result.wrap = *wrap;
    }
    return result;
}

core::Json Manager::textureOptionsToJson(graphics::Texture::Options options) {
    return {{"filter", graphics::Texture::filterName(options.filter)}, {"wrap", graphics::Texture::wrapName(options.wrap)}};
}

Manager::Manager(io::Package& contentPackage, core::JobSystem& jobSystem, graphics::Device& graphicsDevice, core::EventBus& eventBus) : package(contentPackage), jobs(jobSystem), device(graphicsDevice), publisher(std::make_shared<Publisher>(eventBus)) {
    // clang-format off
    registerType({
        .name = "texture",
        .extensions = {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".gif"},
        .normalize = [](const core::Json& options) { return textureOptionsToJson(textureOptionsFromJson(options)); },
        .decode = [](Request& request) -> std::shared_ptr<void> {
            return std::make_shared<graphics::Image>(graphics::Image::decode(request.bytes));
        },
        .finalize = [this](std::shared_ptr<void> decoded, const Request& request) -> std::shared_ptr<void> {
            const graphics::Image& image = *std::static_pointer_cast<graphics::Image>(decoded);
            return device.createTexture(image, textureOptionsFromJson(request.options)).getResource();
        },
        .reload = [this](const std::shared_ptr<void>& asset, Request& request) {
            device.replaceTexture(graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(asset)), graphics::Image::decode(request.bytes));
        },
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
            core::JsonValidator::requireKnownKeys(options, {}, "shader asset options");
            return core::Json::object();
        },
        .decode = [](Request& request) -> std::shared_ptr<void> {
            return graphics::Shader::parse(request.bytes).getResource();
        },
        .finalize = [](std::shared_ptr<void> decoded, const Request&) { return decoded; },
        .reload = [](const std::shared_ptr<void>& asset, Request& request) {
            graphics::Shader(std::static_pointer_cast<graphics::ShaderResource>(asset)).replace(graphics::Shader::parse(request.bytes));
        },
    });
    registerType({
        .name = "json",
        .extensions = {".json"},
        .normalize = [](const core::Json& options) {
            core::JsonValidator::requireKnownKeys(options, {}, "JSON asset options");
            return core::Json::object();
        },
        .decode = [](Request& request) -> std::shared_ptr<void> {
            return std::make_shared<core::Json>(core::Json::parse(request.bytes.begin(), request.bytes.end()));
        },
        .finalize = [](std::shared_ptr<void> decoded, const Request&) { return decoded; },
    });
    // clang-format on
}

Manager::~Manager() {
    *alive = false;
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
    std::size_t reloaded = 0;
    for (auto entry = cache.begin(); entry != cache.end();) {
        // Cache keys read `type|path|options`, and normalized paths never hold the separator.
        const std::string& key = entry->first;
        const std::size_t first = key.find('|');
        const std::size_t second = key.find('|', first + 1);
        if (key.compare(first + 1, second - first - 1, changed) != 0) {
            ++entry;
            continue;
        }

        const std::shared_ptr<void> asset = entry->second.lock();
        if (!asset) {
            entry = cache.erase(entry);
            continue;
        }
        ++reloaded;
        const std::string typeName = key.substr(0, first);
        const Type& kind = getType(typeName);
        if (!kind.reload) {
            entry = cache.erase(entry);
            continue;
        }
        Request fresh = makeRequest(typeName, changed, core::Json::parse(key.substr(second + 1)));
        fresh.bytes = package.readAsset(changed);
        kind.reload(asset, fresh);
        publisher->publish(core::LifecycleEvent::kAssetReloaded, typeName, changed);
        ++entry;
    }
    return reloaded;
}

std::string Manager::cacheKey(std::string_view type, std::string_view path, const core::Json& options) {
    return std::string(type) + "|" + std::string(path) + "|" + options.dump();
}

Manager::Request Manager::makeRequest(std::string_view typeName, std::string_view path, const core::Json& options) const {
    return {.type = std::string(typeName), .path = io::Path::normalize(path), .options = normalizeOptions(typeName, options), .package = &package};
}

core::Json Manager::normalizeOptions(std::string_view typeName, const core::Json& options) const {
    return getType(typeName).normalize(options);
}

std::shared_ptr<void> Manager::load(std::string_view typeName, std::string_view path, const core::Json& options) {
    const Type& assetType = getType(typeName);
    // clang-format off
    return share(typeName, path, options, [this, &assetType](const Request& shared) {
        Request loading = shared;
        loading.bytes = package.readAsset(loading.path);
        return assetType.finalize(assetType.decode(loading), loading);
    });
    // clang-format on
}

std::shared_ptr<void> Manager::share(std::string_view typeName, std::string_view path, const core::Json& options, const std::function<std::shared_ptr<void>(const Request&)>& make) {
    const Request request = makeRequest(typeName, path, options);
    const std::string key = cacheKey(request.type, request.path, request.options);
    if (const auto found = cache.find(key); found != cache.end()) {
        if (std::shared_ptr<void> cached = found->second.lock()) {
            return cached;
        }
    }

    std::shared_ptr<void> asset = track(make(request), request);
    cache.insert_or_assign(key, asset);
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
    auto request = std::make_shared<Request>(makeRequest(typeName, path, options));
    const std::string key = cacheKey(request->type, request->path, request->options);

    // Callbacks always run later on the frame thread, so callers never see them run inside the call itself.
    if (const auto found = cache.find(key); found != cache.end()) {
        if (std::shared_ptr<void> cached = found->second.lock()) {
            jobs.postToFrame([callback = std::move(callback), cached] { callback(cached, {}); });
            return;
        }
    }

    const bool alreadyLoading = pending.contains(key);
    pending[key].callbacks.push_back(std::move(callback));
    if (alreadyLoading) {
        return;
    }

    const Type& assetType = getType(typeName);

    // The file is read on the I/O pool, where waiting on the disk blocks no decoding, then decoded on the task pool and queued on the frame thread, where `finalizePending` creates its GPU resources within the upload budget.
    // clang-format off
    const auto deliver = [this, owner = std::weak_ptr<bool>(alive), key, request, finalize = assetType.finalize](std::shared_ptr<void> decoded, std::string error) {
        jobs.postToFrame([this, owner, key, request, finalize, decoded = std::move(decoded), error = std::move(error)]() mutable {
            if (!owner.expired()) {
                uploads.push_back({.key = key, .request = request, .finalize = std::move(finalize), .decoded = std::move(decoded), .error = std::move(error)});
            }
        });
    };

    jobs.postIo([this, request, decode = assetType.decode, deliver] {
        try {
            request->bytes = package.readAsset(request->path);
        } catch (const std::exception& exception) {
            deliver(nullptr, exception.what());
            return;
        }

        jobs.post([request, decode, deliver] {
            std::shared_ptr<void> decoded;
            std::string error;
            try {
                decoded = decode(*request);
            } catch (const std::exception& exception) {
                error = exception.what();
            }
            deliver(std::move(decoded), std::move(error));
        });
    });
    // clang-format on
}

void Manager::finish(const std::string& key, std::shared_ptr<void> asset, const std::string& error) {
    std::vector<Callback> callbacks;
    if (const auto found = pending.find(key); found != pending.end()) {
        callbacks = std::move(found->second.callbacks);
        pending.erase(found);
    }

    if (asset) {
        cache.insert_or_assign(key, asset);
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

std::shared_ptr<text::Font> Manager::font(std::string_view path, text::TrueTypeFont::Options options) {
    return std::static_pointer_cast<text::Font>(load("font", path, fontOptionsToJson(options)));
}

core::Json Manager::json(std::string_view path) {
    return *std::static_pointer_cast<core::Json>(load("json", path));
}

std::string Manager::text(std::string_view path) const {
    return package.readAssetText(path);
}

std::vector<std::uint8_t> Manager::bytes(std::string_view path) const {
    return package.readAsset(path);
}

std::vector<std::uint8_t> Manager::bytes(std::string_view path, std::uint64_t offset, std::size_t size) const {
    return package.readAssetRange(path, offset, size);
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

void Manager::defineGroup(std::string name, std::vector<Entry> entries) {
    Group& group = groups[std::move(name)];
    group.entries = std::move(entries);
}

void Manager::defineGroups(const core::Json& manifest) {
    core::JsonValidator::requireKnownKeys(manifest, {"groups"}, "the asset group manifest");
    for (const auto& [name, items] : manifest.at("groups").items()) {
        std::vector<Entry> entries;
        for (const core::Json& item : items) {
            if (item.is_string()) {
                entries.push_back({.path = item.get<std::string>()});
                continue;
            }
            core::JsonValidator::requireKnownKeys(item, {"path", "type", "options"}, "an asset group entry");
            entries.push_back({.path = item.at("path").get<std::string>(), .type = item.value("type", std::string{}), .options = item.value("options", core::Json::object())});
        }
        defineGroup(name, std::move(entries));
    }
}

std::vector<Manager::Entry> Manager::expand(const std::vector<Entry>& entries) const {
    std::vector<Entry> expanded;
    for (const Entry& entry : entries) {
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

    if (progress) {
        group.progressListeners.push_back(std::move(progress));
    }
    if (completion) {
        group.completionListeners.push_back(std::move(completion));
    }
    if (group.loading) {
        return;
    }

    const std::vector<Entry> entries = expand(group.entries);
    group.held.clear();
    group.errors.clear();
    group.completed = 0;
    group.total = entries.size();
    group.loading = true;
    const std::uint64_t generation = ++group.generation;
    const std::string groupName(name);

    if (entries.empty()) {
        jobs.postToFrame([this, groupName, generation] { completeGroup(groupName, generation); });
        return;
    }

    for (const Entry& entry : entries) {
        // clang-format off
        loadAsync(entry.type, entry.path, [this, groupName, generation, path = entry.path](std::shared_ptr<void> asset, std::string error) {
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

            const float fraction = static_cast<float>(target.completed) / static_cast<float>(target.total);
            const std::vector<GroupProgress> listeners = target.progressListeners;
            for (const GroupProgress& listener : listeners) {
                listener(fraction);
            }
            if (target.completed == target.total) {
                completeGroup(groupName, generation);
            }
        }, entry.options);
        // clang-format on
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
    group.held.clear();
    group.errors.clear();
    group.progressListeners.clear();
    group.completionListeners.clear();
    group.completed = 0;
    group.loading = false;
    group.loaded = false;
    ++group.generation;
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
    return static_cast<std::size_t>(std::count_if(cache.begin(), cache.end(), [](const auto& entry) { return !entry.second.expired(); }));
}

std::size_t Manager::getPendingCount() const noexcept {
    return pending.size();
}

std::size_t Manager::releaseUnused() {
    return static_cast<std::size_t>(std::erase_if(cache, [](const auto& entry) { return entry.second.expired(); }));
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

        std::shared_ptr<void> asset;
        if (upload.error.empty()) {
            try {
                asset = track(upload.finalize(std::move(upload.decoded), *upload.request), *upload.request);
            } catch (const std::exception& exception) {
                upload.error = exception.what();
            }
        }
        finish(upload.key, std::move(asset), upload.error);
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
