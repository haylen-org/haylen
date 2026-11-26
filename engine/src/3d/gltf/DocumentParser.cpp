#include "3d/gltf/DocumentParser.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

#include "3d/gltf/FormatNames.hpp"
#include "3d/gltf/UriResolver.hpp"
#include "haylen/3d/gltf/Reader.hpp"
#include "haylen/math/Mat4.hpp"

namespace haylen::gltf {

using Problem = ReadError::Problem;

DocumentParser::DocumentParser(const core::Json& documentRoot, const Source& documentSource, const Limits& readerLimits, const Diagnostics& documentDiagnostics) : root(documentRoot), source(documentSource), limits(readerLimits), diagnostics(documentDiagnostics) {}

Document DocumentParser::parse(const core::Json& root, const Source& source, const Limits& limits, const Diagnostics& diagnostics) {
    if (!root.is_object()) {
        diagnostics.invalid(Problem::WrongType, "its JSON holds " + std::string(describeType(root)) + " where an object belongs");
    }

    DocumentParser parser(root, source, limits, diagnostics);
    parser.document.file = diagnostics.getFile();

    // The version and the extensions come first, so a file that needs a newer reader or an extension it lacks says so before any rule of its objects.
    parser.readAsset();
    parser.readExtensionLists();

    // clang-format off
    const auto count = [&parser](std::string_view key) {
        const core::Json* list = parser.find(parser.root, key);
        return list != nullptr && list->is_array() ? list->size() : std::size_t{0};
    };
    // clang-format on
    parser.counts = {
        .accessors = count("accessors"),
        .animations = count("animations"),
        .buffers = count("buffers"),
        .bufferViews = count("bufferViews"),
        .cameras = count("cameras"),
        .images = count("images"),
        .materials = count("materials"),
        .meshes = count("meshes"),
        .nodes = count("nodes"),
        .samplers = count("samplers"),
        .scenes = count("scenes"),
        .skins = count("skins"),
        .textures = count("textures"),
    };

    parser.readBuffers();
    parser.readBufferViews();
    parser.readAccessors();
    parser.readImages();
    parser.readSamplers();
    parser.readTextures();
    parser.readMaterials();
    parser.readMeshes();
    parser.readNodes();
    parser.readSkins();
    parser.readCameras();
    parser.readAnimations();
    parser.readScenes();
    parser.document.scene = parser.readOptionalIndex(root, "scene", "the document", parser.counts.scenes, "scene");
    parser.document.extras = parser.readExtras(root);
    parser.document.extensions = parser.readExtensions(root, "the document");
    return std::move(parser.document);
}

void DocumentParser::readAsset() {
    const core::Json* asset = find(root, "asset");
    if (asset == nullptr) {
        diagnostics.invalid(Problem::MissingProperty, "it lacks the key \"asset\", which names the version of glTF it follows");
    }

    const std::string owner = "the asset";
    if (!asset->is_object()) {
        wrongType("asset", "the document", "an object");
    }

    // clang-format off
    const auto parseVersion = [this, &owner](std::string_view key, const std::string& text) {
        const std::size_t dot = text.find('.');
        const bool digits = dot != std::string::npos && dot > 0 && dot + 1 < text.size() && std::ranges::all_of(text, [](char character) { return character == '.' || (character >= '0' && character <= '9'); });
        if (!digits || text.find('.', dot + 1) != std::string::npos || text.size() > 20) {
            diagnostics.invalid(Problem::InvalidValue, Diagnostics::key(key, owner) + " holds \"" + text + "\", which is not a version such as \"2.0\"");
        }

        return std::pair(std::stoull(text.substr(0, dot)), std::stoull(text.substr(dot + 1)));
    };
    // clang-format on

    document.asset.version = readString(*asset, "version", owner);
    const auto version = parseVersion("version", document.asset.version);
    if (version.first != 2) {
        diagnostics.refuse(Problem::UnsupportedVersion, "follows glTF " + document.asset.version + ", while Haylen reads glTF 2.0", "Export the model as glTF 2.0.");
    }

    if (const std::optional<std::string> minVersion = readOptionalString(*asset, "minVersion", owner)) {
        document.asset.minVersion = *minVersion;
        const auto minimum = parseVersion("minVersion", *minVersion);
        if (minimum > version) {
            diagnostics.invalid(Problem::InvalidValue, "the oldest version that reads it, " + *minVersion + ", is newer than the version it follows, " + document.asset.version);
        }

        if (minimum.first != 2 || minimum.second > 0) {
            diagnostics.refuse(Problem::UnsupportedVersion, "needs a reader of glTF " + *minVersion + ", while Haylen reads glTF 2.0", "Export the model as glTF 2.0.");
        }
    }

    document.asset.generator = readOptionalString(*asset, "generator", owner).value_or("");
    document.asset.copyright = readOptionalString(*asset, "copyright", owner).value_or("");
    document.asset.extras = readExtras(*asset);
}

void DocumentParser::readExtensionLists() {
    // clang-format off
    const auto readNames = [this](std::string_view key) {
        std::vector<std::string> names;
        const core::Json* list = find(root, key);
        if (list == nullptr) {
            return names;
        }

        if (!list->is_array()) {
            wrongType(key, "the document", "an array");
        }

        if (list->empty()) {
            diagnostics.invalid(Problem::EmptyElement, "its list \"" + std::string(key) + "\" is empty");
        }

        countObjects(list->size());
        for (const core::Json& name : *list) {
            if (!name.is_string()) {
                wrongType(key, "the document", "an array of strings");
            }

            if (std::ranges::find(names, name.get<std::string>()) != names.end()) {
                diagnostics.invalid(Problem::DuplicateElement, "its list \"" + std::string(key) + "\" names the extension \"" + name.get<std::string>() + "\" twice");
            }

            names.push_back(name.get<std::string>());
        }

        return names;
    };
    // clang-format on

    document.extensionsUsed = readNames("extensionsUsed");
    document.extensionsRequired = readNames("extensionsRequired");
    for (const std::string& extension : document.extensionsRequired) {
        if (!isUsed(extension)) {
            diagnostics.invalid(Problem::UndeclaredExtension, "it requires the extension \"" + extension + "\", which its list \"extensionsUsed\" lacks");
        }

        if (extension == "EXT_texture_avif") {
            diagnostics.refuse(Problem::UnsupportedImageFormat, "requires images in the AVIF format, which Haylen does not read", "Export the textures of the model as PNG, JPEG or WebP images.");
        }

        if (std::ranges::find(Reader::kSupportedExtensions, extension) == Reader::kSupportedExtensions.end()) {
            diagnostics.refuse(Problem::UnsupportedExtension, "requires the extension \"" + extension + "\", which Haylen does not support", "Export the model again without that extension.");
        }
    }
}

void DocumentParser::readBuffers() {
    const std::vector<const core::Json*> items = readList(root, "buffers", "buffer", "the document", false);
    std::uint64_t total = 0;
    for (std::size_t index = 0; index < items.size(); ++index) {
        const core::Json& object = *items[index];
        const std::string owner = Diagnostics::item("buffer", index);
        Buffer buffer;
        buffer.name = readOptionalString(object, "name", owner).value_or("");
        buffer.byteLength = readInteger(require(object, "byteLength", owner), "byteLength", owner, 1, std::numeric_limits<std::uint64_t>::max());
        if (buffer.byteLength > limits.maxBufferBytes - std::min(total, limits.maxBufferBytes)) {
            diagnostics.refuse(Problem::LimitExceeded, "holds buffers of more than the " + std::to_string(limits.maxBufferBytes) + " bytes the reader takes from one file", "Split the model into smaller files.");
        }

        total += buffer.byteLength;
        const std::optional<std::string> uri = readOptionalString(object, "uri", owner);
        if (!uri) {
            if (index != 0 || !source.glb) {
                diagnostics.invalid(Problem::MissingProperty, owner + " names no URI, which only the first buffer of a GLB file may leave out to take the binary chunk");
            }

            if (!source.binaryLength) {
                diagnostics.invalid(Problem::GlbMissingBinary, "its first buffer takes the binary chunk, which the GLB file lacks");
            }

            if (*source.binaryLength < buffer.byteLength) {
                diagnostics.invalid(Problem::ResourceTooShort, "the binary chunk holds " + std::to_string(*source.binaryLength) + " bytes, fewer than the " + std::to_string(buffer.byteLength) + " bytes of buffer 0");
            }

            buffer.storage = Buffer::Storage::Glb;
        } else if (UriResolver::isDataUri(*uri)) {
            const UriResolver::DataUri data = UriResolver::parseDataUri(*uri, owner, diagnostics);
            if (data.mediaType != "application/octet-stream" && data.mediaType != "application/gltf-buffer") {
                diagnostics.invalid(Problem::InvalidUri, "the \"data:\" URI of " + owner + " has the media type \"" + data.mediaType + "\", while a buffer takes \"application/octet-stream\" or \"application/gltf-buffer\"");
            }

            buffer.storage = Buffer::Storage::DataUri;
            buffer.uri = *uri;
        } else {
            buffer.storage = Buffer::Storage::File;
            buffer.uri = *uri;
            buffer.path = UriResolver::resolve(*uri, source.directory, owner, diagnostics);
            if (!source.filesAllowed) {
                diagnostics.refuse(Problem::FileOutsideDocument, "names the file \"" + *uri + "\" for " + owner + ", while bytes in memory read no file beside them", "Send the model as a GLB file, which holds its buffers and images.");
            }
        }

        buffer.extras = readExtras(object);
        buffer.extensions = readExtensions(object, owner);
        document.buffers.push_back(std::move(buffer));
    }
}

void DocumentParser::readBufferViews() {
    const std::vector<const core::Json*> items = readList(root, "bufferViews", "buffer view", "the document", false);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const core::Json& object = *items[index];
        const std::string owner = Diagnostics::item("buffer view", index);
        BufferView view;
        view.name = readOptionalString(object, "name", owner).value_or("");
        view.buffer = readIndex(object, "buffer", owner, counts.buffers, "buffer");
        view.byteOffset = readOptionalInteger(object, "byteOffset", owner, 0, std::numeric_limits<std::uint64_t>::max()).value_or(0);
        view.byteLength = readInteger(require(object, "byteLength", owner), "byteLength", owner, 1, std::numeric_limits<std::uint64_t>::max());
        const std::uint64_t bufferLength = document.buffers[view.buffer].byteLength;
        if (view.byteOffset > bufferLength || view.byteLength > bufferLength - view.byteOffset) {
            diagnostics.invalid(Problem::BufferViewOutOfRange, owner + " takes the bytes " + std::to_string(view.byteOffset) + " to " + std::to_string(view.byteOffset + view.byteLength) + " of buffer " + std::to_string(view.buffer) + ", which holds " + std::to_string(bufferLength));
        }

        if (const std::optional<std::uint64_t> stride = readOptionalInteger(object, "byteStride", owner, 4, 252)) {
            if (*stride % 4 != 0 || *stride > view.byteLength) {
                diagnostics.invalid(Problem::InvalidByteStride, "the stride of " + owner + ", " + std::to_string(*stride) + " bytes, is not a multiple of four within its length");
            }

            view.byteStride = static_cast<std::uint32_t>(*stride);
        }

        if (const core::Json* target = find(object, "target")) {
            const std::uint64_t value = readInteger(*target, "target", owner, 0, std::numeric_limits<std::uint64_t>::max());
            view.target = FormatNames::findTarget(value);
            if (!view.target) {
                diagnostics.invalid(Problem::InvalidValue, Diagnostics::key("target", owner) + " holds " + std::to_string(value) + ", which is neither 34962 for vertices nor 34963 for indices");
            }
        }

        view.extras = readExtras(object);
        view.extensions = readExtensions(object, owner);
        document.bufferViews.push_back(std::move(view));
    }
}

void DocumentParser::readAccessors() {
    const std::vector<const core::Json*> items = readList(root, "accessors", "accessor", "the document", false);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const core::Json& object = *items[index];
        const std::string owner = Diagnostics::item("accessor", index);
        Accessor accessor;
        accessor.name = readOptionalString(object, "name", owner).value_or("");
        accessor.bufferView = readOptionalIndex(object, "bufferView", owner, counts.bufferViews, "buffer view");

        const std::uint64_t componentType = readInteger(require(object, "componentType", owner), "componentType", owner, 0, std::numeric_limits<std::uint64_t>::max());
        const std::optional<Accessor::ComponentType> component = FormatNames::findComponentType(componentType);
        if (!component) {
            diagnostics.invalid(Problem::InvalidValue, Diagnostics::key("componentType", owner) + " holds " + std::to_string(componentType) + ", which is no component type of glTF");
        }

        accessor.componentType = *component;
        accessor.count = readInteger(require(object, "count", owner), "count", owner, 1, std::numeric_limits<std::uint64_t>::max());
        const std::string type = readString(object, "type", owner);
        const std::optional<Accessor::Type> found = FormatNames::findAccessorType(type);
        if (!found) {
            diagnostics.invalid(Problem::InvalidValue, Diagnostics::key("type", owner) + " holds \"" + type + "\", which is none of \"SCALAR\", \"VEC2\", \"VEC3\", \"VEC4\", \"MAT2\", \"MAT3\" and \"MAT4\"");
        }

        accessor.type = *found;
        accessor.normalized = readBool(object, "normalized", owner);
        if (accessor.normalized && (accessor.componentType == Accessor::ComponentType::Float || accessor.componentType == Accessor::ComponentType::UnsignedInt)) {
            diagnostics.invalid(Problem::InvalidValue, owner + " is normalized, which only bytes and shorts may be");
        }

        // The values that one accessor gives when it is read stay within the bytes the reader holds, also for accessors without a buffer view.
        const std::uint64_t components = accessor.getComponentCount();
        if (accessor.count > limits.maxBufferBytes / (components * sizeof(float))) {
            diagnostics.refuse(Problem::LimitExceeded, "holds " + owner + " with " + std::to_string(accessor.count) + " elements, more than the reader takes", "Split the model into smaller files.");
        }

        const std::size_t componentSize = Accessor::getComponentSize(accessor.componentType);
        if (const std::optional<std::uint64_t> offset = readOptionalInteger(object, "byteOffset", owner, 0, std::numeric_limits<std::uint64_t>::max())) {
            if (!accessor.bufferView) {
                diagnostics.invalid(Problem::InvalidValue, owner + " has a byte offset without a buffer view");
            }

            accessor.byteOffset = *offset;
        }

        if (accessor.bufferView) {
            const BufferView& view = document.bufferViews[*accessor.bufferView];
            const bool matrix = accessor.type == Accessor::Type::Mat2 || accessor.type == Accessor::Type::Mat3 || accessor.type == Accessor::Type::Mat4;
            if (accessor.byteOffset % componentSize != 0 || (view.byteOffset + accessor.byteOffset) % componentSize != 0 || (matrix && accessor.byteOffset % 4 != 0)) {
                diagnostics.invalid(Problem::Misaligned, owner + " starts at an offset that is not a multiple of the size of its components");
            }

            const std::uint64_t elementSize = accessor.getElementSize();
            const std::uint64_t stride = view.byteStride.value_or(static_cast<std::uint32_t>(elementSize));
            if (view.byteStride && (*view.byteStride < elementSize || *view.byteStride % componentSize != 0)) {
                diagnostics.invalid(Problem::InvalidByteStride, "the stride of buffer view " + std::to_string(*accessor.bufferView) + " does not fit the elements of " + owner);
            }

            const std::uint64_t last = accessor.count - 1;
            const bool fits = accessor.byteOffset <= view.byteLength && last <= (view.byteLength - accessor.byteOffset) / stride && elementSize <= view.byteLength - accessor.byteOffset - last * stride;
            if (!fits) {
                diagnostics.invalid(Problem::AccessorOutOfRange, owner + " reads past the end of buffer view " + std::to_string(*accessor.bufferView));
            }
        }

        // clang-format off
        const auto readBounds = [&](std::string_view key) {
            std::vector<double> bounds;
            const core::Json* list = find(object, key);
            if (list == nullptr) {
                return bounds;
            }

            if (!list->is_array() || list->size() != components) {
                diagnostics.invalid(Problem::WrongType, Diagnostics::key(key, owner) + " does not hold one number for each of its " + std::to_string(components) + " components");
            }

            for (const core::Json& value : *list) {
                const double number = readNumber(value, key, owner);
                if (accessor.componentType == Accessor::ComponentType::Float) {
                    bounds.push_back(static_cast<double>(readFloat(value, key, owner)));
                    continue;
                }

                if (number != std::floor(number) || number < FormatNames::getLowest(accessor.componentType) || number > FormatNames::getHighest(accessor.componentType)) {
                    diagnostics.invalid(Problem::InvalidValue, Diagnostics::key(key, owner) + " holds " + value.dump() + ", which its components cannot store");
                }

                bounds.push_back(number);
            }

            return bounds;
        };
        // clang-format on
        accessor.min = readBounds("min");
        accessor.max = readBounds("max");

        if (const core::Json* sparse = findObject(object, "sparse", owner)) {
            accessor.sparse = readSparse(*sparse, owner);
            if (accessor.sparse->count > accessor.count) {
                diagnostics.invalid(Problem::InvalidValue, "the sparse part of " + owner + " replaces " + std::to_string(accessor.sparse->count) + " elements, more than the " + std::to_string(accessor.count) + " it holds");
            }

            const std::uint64_t indexSize = Accessor::getComponentSize(accessor.sparse->indicesComponentType);
            const std::uint64_t elementSize = accessor.getElementSize();
            // clang-format off
            const auto checkPart = [&](std::size_t viewIndex, std::uint64_t offset, std::uint64_t size, std::uint64_t alignment, std::string_view part) {
                const BufferView& view = document.bufferViews[viewIndex];
                if (view.byteStride || view.target) {
                    diagnostics.invalid(Problem::InvalidByteStride, "buffer view " + std::to_string(viewIndex) + " holds the sparse " + std::string(part) + " of " + owner + " and so takes no stride and no target");
                }

                if (offset % alignment != 0 || (view.byteOffset + offset) % alignment != 0) {
                    diagnostics.invalid(Problem::Misaligned, "the sparse " + std::string(part) + " of " + owner + " start at an offset that is not a multiple of the size of their components");
                }

                if (offset > view.byteLength || accessor.sparse->count > (view.byteLength - offset) / size) {
                    diagnostics.invalid(Problem::AccessorOutOfRange, "the sparse " + std::string(part) + " of " + owner + " read past the end of buffer view " + std::to_string(viewIndex));
                }
            };
            // clang-format on
            checkPart(accessor.sparse->indicesBufferView, accessor.sparse->indicesByteOffset, indexSize, indexSize, "indices");
            checkPart(accessor.sparse->valuesBufferView, accessor.sparse->valuesByteOffset, elementSize, componentSize, "values");
        }

        accessor.extras = readExtras(object);
        accessor.extensions = readExtensions(object, owner);
        document.accessors.push_back(std::move(accessor));
    }
}

Accessor::Sparse DocumentParser::readSparse(const core::Json& object, const std::string& owner) {
    const std::string sparseOwner = "the sparse part of " + owner;
    Accessor::Sparse sparse;
    sparse.count = readInteger(require(object, "count", sparseOwner), "count", sparseOwner, 1, std::numeric_limits<std::uint64_t>::max());

    const std::string indicesOwner = "the sparse indices of " + owner;
    const core::Json* indices = findObject(object, "indices", sparseOwner);
    if (indices == nullptr) {
        diagnostics.invalid(Problem::MissingProperty, Diagnostics::key("indices", sparseOwner) + " is missing");
    }

    sparse.indicesBufferView = readIndex(*indices, "bufferView", indicesOwner, counts.bufferViews, "buffer view");
    sparse.indicesByteOffset = readOptionalInteger(*indices, "byteOffset", indicesOwner, 0, std::numeric_limits<std::uint64_t>::max()).value_or(0);
    const std::uint64_t componentType = readInteger(require(*indices, "componentType", indicesOwner), "componentType", indicesOwner, 0, std::numeric_limits<std::uint64_t>::max());
    const std::optional<Accessor::ComponentType> component = FormatNames::findComponentType(componentType);
    if (!component || (*component != Accessor::ComponentType::UnsignedByte && *component != Accessor::ComponentType::UnsignedShort && *component != Accessor::ComponentType::UnsignedInt)) {
        diagnostics.invalid(Problem::InvalidValue, Diagnostics::key("componentType", indicesOwner) + " holds " + std::to_string(componentType) + ", while sparse indices are unsigned bytes, shorts or integers");
    }

    sparse.indicesComponentType = *component;
    (void)readExtensions(*indices, indicesOwner);

    const std::string valuesOwner = "the sparse values of " + owner;
    const core::Json* values = findObject(object, "values", sparseOwner);
    if (values == nullptr) {
        diagnostics.invalid(Problem::MissingProperty, Diagnostics::key("values", sparseOwner) + " is missing");
    }

    sparse.valuesBufferView = readIndex(*values, "bufferView", valuesOwner, counts.bufferViews, "buffer view");
    sparse.valuesByteOffset = readOptionalInteger(*values, "byteOffset", valuesOwner, 0, std::numeric_limits<std::uint64_t>::max()).value_or(0);
    (void)readExtensions(*values, valuesOwner);
    sparse.extras = readExtras(object);
    sparse.extensions = readExtensions(object, sparseOwner);
    return sparse;
}

void DocumentParser::readImages() {
    const std::vector<const core::Json*> items = readList(root, "images", "image", "the document", false);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const core::Json& object = *items[index];
        const std::string owner = Diagnostics::item("image", index);
        Image image;
        image.name = readOptionalString(object, "name", owner).value_or("");
        const std::optional<std::string> uri = readOptionalString(object, "uri", owner);
        image.bufferView = readOptionalIndex(object, "bufferView", owner, counts.bufferViews, "buffer view");
        if (uri.has_value() == image.bufferView.has_value()) {
            diagnostics.invalid(Problem::InvalidValue, owner + " names " + std::string(uri ? "both a URI and a buffer view" : "neither a URI nor a buffer view") + ", while it takes exactly one of them");
        }

        if (const std::optional<std::string> mimeType = readOptionalString(object, "mimeType", owner)) {
            image.mimeType = *mimeType;
        } else if (image.bufferView) {
            diagnostics.invalid(Problem::MissingProperty, owner + " lies in a buffer view without the key \"mimeType\" that names its format");
        }

        if (image.bufferView) {
            image.storage = Buffer::Storage::BufferView;
        } else if (UriResolver::isDataUri(*uri)) {
            const UriResolver::DataUri data = UriResolver::parseDataUri(*uri, owner, diagnostics);
            if (!image.mimeType.empty() && image.mimeType != data.mediaType) {
                diagnostics.invalid(Problem::InvalidImage, "the \"data:\" URI of " + owner + " has the media type \"" + data.mediaType + "\", while the image names \"" + image.mimeType + "\"");
            }

            image.mimeType = data.mediaType;
            image.storage = Buffer::Storage::DataUri;
            image.uri = *uri;
        } else {
            image.storage = Buffer::Storage::File;
            image.uri = *uri;
            image.path = UriResolver::resolve(*uri, source.directory, owner, diagnostics);
            if (!source.filesAllowed) {
                diagnostics.refuse(Problem::FileOutsideDocument, "names the file \"" + *uri + "\" for " + owner + ", while bytes in memory read no file beside them", "Send the model as a GLB file, which holds its buffers and images.");
            }
        }

        if (!image.mimeType.empty() && !FormatNames::isImageTypeEnabled(image.mimeType, document.extensionsUsed)) {
            diagnostics.invalid(Problem::InvalidImage, owner + " has the media type \"" + image.mimeType + "\", which only an extension that the file does not use enables");
        }

        image.extras = readExtras(object);
        image.extensions = readExtensions(object, owner);
        document.images.push_back(std::move(image));
    }
}

void DocumentParser::readSamplers() {
    const std::vector<const core::Json*> items = readList(root, "samplers", "sampler", "the document", false);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const core::Json& object = *items[index];
        const std::string owner = Diagnostics::item("sampler", index);
        Sampler sampler;
        sampler.name = readOptionalString(object, "name", owner).value_or("");
        // clang-format off
        const auto readFilter = [&](std::string_view key, bool magnifies) -> std::optional<Sampler::Filter> {
            const std::optional<std::uint64_t> value = readOptionalInteger(object, key, owner, 0, std::numeric_limits<std::uint64_t>::max());
            if (!value) {
                return std::nullopt;
            }

            const std::optional<Sampler::Filter> filter = FormatNames::findFilter(*value);
            const bool plain = filter == Sampler::Filter::Nearest || filter == Sampler::Filter::Linear;
            if (!filter || (magnifies && !plain)) {
                diagnostics.invalid(Problem::InvalidValue, Diagnostics::key(key, owner) + " holds " + std::to_string(*value) + ", which is no filter of glTF for it");
            }

            return filter;
        };
        const auto readWrap = [&](std::string_view key) {
            const std::optional<std::uint64_t> value = readOptionalInteger(object, key, owner, 0, std::numeric_limits<std::uint64_t>::max());
            if (!value) {
                return Sampler::Wrap::Repeat;
            }

            const std::optional<Sampler::Wrap> wrap = FormatNames::findWrap(*value);
            if (!wrap) {
                diagnostics.invalid(Problem::InvalidValue, Diagnostics::key(key, owner) + " holds " + std::to_string(*value) + ", which is none of 33071, 33648 and 10497");
            }

            return *wrap;
        };
        // clang-format on
        sampler.magFilter = readFilter("magFilter", true);
        sampler.minFilter = readFilter("minFilter", false);
        sampler.wrapS = readWrap("wrapS");
        sampler.wrapT = readWrap("wrapT");
        sampler.extras = readExtras(object);
        sampler.extensions = readExtensions(object, owner);
        document.samplers.push_back(std::move(sampler));
    }
}

void DocumentParser::readTextures() {
    const std::vector<const core::Json*> items = readList(root, "textures", "texture", "the document", false);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const core::Json& object = *items[index];
        const std::string owner = Diagnostics::item("texture", index);
        Texture texture;
        texture.name = readOptionalString(object, "name", owner).value_or("");
        texture.sampler = readOptionalIndex(object, "sampler", owner, counts.samplers, "sampler");
        texture.source = readOptionalIndex(object, "source", owner, counts.images, "image");
        texture.extras = readExtras(object);
        texture.extensions = readExtensions(object, owner);

        // The image of the extension of WebP textures must be a WebP image of the file.
        if (const core::Json* webp = texture.extensions.is_object() && texture.extensions.contains("EXT_texture_webp") ? &texture.extensions.at("EXT_texture_webp") : nullptr) {
            const std::string webpOwner = "the WebP image of " + owner;
            if (!webp->is_object()) {
                wrongType("EXT_texture_webp", owner, "an object");
            }

            (void)readIndex(*webp, "source", webpOwner, counts.images, "image");
        }

        document.textures.push_back(std::move(texture));
    }
}

std::optional<Material::TextureInfo> DocumentParser::readTextureInfo(const core::Json& object, std::string_view key, const std::string& owner) {
    const std::string infoOwner = Diagnostics::key(key, owner);
    const core::Json* info = findObject(object, key, owner);
    if (info == nullptr) {
        return std::nullopt;
    }

    Material::TextureInfo texture;
    texture.index = readIndex(*info, "index", infoOwner, counts.textures, "texture");
    texture.texCoord = static_cast<std::uint32_t>(readOptionalInteger(*info, "texCoord", infoOwner, 0, 999999999).value_or(0));
    texture.extras = readExtras(*info);
    texture.extensions = readExtensions(*info, infoOwner);
    return texture;
}

void DocumentParser::readMaterials() {
    const std::vector<const core::Json*> items = readList(root, "materials", "material", "the document", false);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const core::Json& object = *items[index];
        const std::string owner = Diagnostics::item("material", index);
        Material material;
        material.name = readOptionalString(object, "name", owner).value_or("");
        // clang-format off
        const auto readUnit = [&](const core::Json& holder, std::string_view key, const std::string& holderName, float fallback) {
            const float value = readOptionalFloat(holder, key, holderName).value_or(fallback);
            requireRange(value, 0.0, 1.0, key, holderName);
            return value;
        };
        // clang-format on

        if (const core::Json* pbr = findObject(object, "pbrMetallicRoughness", owner)) {
            const std::string pbrOwner = Diagnostics::key("pbrMetallicRoughness", owner);
            if (find(*pbr, "baseColorFactor") != nullptr) {
                const std::vector<float> factor = readFloats(*pbr, "baseColorFactor", pbrOwner, 4);
                for (const float component : factor) {
                    requireRange(component, 0.0, 1.0, "baseColorFactor", pbrOwner);
                }

                material.baseColorFactor = {factor[0], factor[1], factor[2], factor[3]};
            }

            material.baseColorTexture = readTextureInfo(*pbr, "baseColorTexture", pbrOwner);
            material.metallicFactor = readUnit(*pbr, "metallicFactor", pbrOwner, 1.0F);
            material.roughnessFactor = readUnit(*pbr, "roughnessFactor", pbrOwner, 1.0F);
            material.metallicRoughnessTexture = readTextureInfo(*pbr, "metallicRoughnessTexture", pbrOwner);
            material.pbrExtras = readExtras(*pbr);
            material.pbrExtensions = readExtensions(*pbr, pbrOwner);
        }

        material.normalTexture = readTextureInfo(object, "normalTexture", owner);
        if (material.normalTexture) {
            material.normalScale = readOptionalFloat(object.at("normalTexture"), "scale", Diagnostics::key("normalTexture", owner)).value_or(1.0F);
        }

        material.occlusionTexture = readTextureInfo(object, "occlusionTexture", owner);
        if (material.occlusionTexture) {
            material.occlusionStrength = readUnit(object.at("occlusionTexture"), "strength", Diagnostics::key("occlusionTexture", owner), 1.0F);
        }

        material.emissiveTexture = readTextureInfo(object, "emissiveTexture", owner);
        if (find(object, "emissiveFactor") != nullptr) {
            const std::vector<float> factor = readFloats(object, "emissiveFactor", owner, 3);
            for (const float component : factor) {
                requireRange(component, 0.0, 1.0, "emissiveFactor", owner);
            }

            material.emissiveFactor = {factor[0], factor[1], factor[2]};
        }

        if (const std::optional<std::string> mode = readOptionalString(object, "alphaMode", owner)) {
            const std::optional<Material::AlphaMode> alphaMode = FormatNames::findAlphaMode(*mode);
            if (!alphaMode) {
                diagnostics.invalid(Problem::InvalidValue, Diagnostics::key("alphaMode", owner) + " holds \"" + *mode + "\", which is none of \"OPAQUE\", \"MASK\" and \"BLEND\"");
            }

            material.alphaMode = *alphaMode;
        }

        material.alphaCutoff = readOptionalFloat(object, "alphaCutoff", owner).value_or(0.5F);
        if (material.alphaCutoff < 0.0F) {
            diagnostics.invalid(Problem::InvalidValue, Diagnostics::key("alphaCutoff", owner) + " is negative");
        }

        material.doubleSided = readBool(object, "doubleSided", owner);
        material.extras = readExtras(object);
        material.extensions = readExtensions(object, owner);
        document.materials.push_back(std::move(material));
    }
}

std::vector<Mesh::Attribute> DocumentParser::readAttributes(const core::Json& object, const std::string& owner) {
    if (!object.is_object()) {
        diagnostics.invalid(Problem::WrongType, owner + " is not an object of attributes");
    }

    std::vector<Mesh::Attribute> attributes;
    for (const auto& [name, value] : object.items()) {
        const std::uint64_t accessor = readInteger(value, name, owner, 0, std::numeric_limits<std::uint64_t>::max());
        requireIndex(accessor, name, owner, counts.accessors, "accessor");
        attributes.push_back({.name = name, .accessor = static_cast<std::size_t>(accessor)});
    }

    countObjects(attributes.size());
    std::ranges::sort(attributes, {}, &Mesh::Attribute::name);
    return attributes;
}

Mesh::Primitive DocumentParser::readPrimitive(const core::Json& object, const std::string& owner) {
    Mesh::Primitive primitive;
    const core::Json& attributes = require(object, "attributes", owner);
    primitive.attributes = readAttributes(attributes, Diagnostics::key("attributes", owner));
    if (primitive.attributes.empty()) {
        diagnostics.invalid(Problem::EmptyElement, "the attributes of " + owner + " are empty");
    }

    primitive.indices = readOptionalIndex(object, "indices", owner, counts.accessors, "accessor");
    primitive.material = readOptionalIndex(object, "material", owner, counts.materials, "material");
    primitive.mode = static_cast<Mesh::Primitive::Mode>(readOptionalInteger(object, "mode", owner, 0, 6).value_or(4));
    if (const core::Json* targets = find(object, "targets")) {
        if (!targets->is_array() || targets->empty()) {
            diagnostics.invalid(Problem::WrongType, Diagnostics::key("targets", owner) + " is not a list of morph targets");
        }

        countObjects(targets->size());
        for (std::size_t target = 0; target < targets->size(); ++target) {
            const std::string targetOwner = Diagnostics::item("morph target", target, owner);
            primitive.targets.push_back(readAttributes(targets->at(target), targetOwner));
            if (primitive.targets.back().empty()) {
                diagnostics.invalid(Problem::EmptyElement, targetOwner + " is empty");
            }
        }
    }

    primitive.extras = readExtras(object);
    primitive.extensions = readExtensions(object, owner);
    return primitive;
}

void DocumentParser::readMeshes() {
    const std::vector<const core::Json*> items = readList(root, "meshes", "mesh", "the document", false);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const core::Json& object = *items[index];
        const std::string owner = Diagnostics::item("mesh", index);
        Mesh mesh;
        mesh.name = readOptionalString(object, "name", owner).value_or("");
        const std::vector<const core::Json*> entries = readList(object, "primitives", "primitive", owner, true);
        for (std::size_t primitive = 0; primitive < entries.size(); ++primitive) {
            mesh.primitives.push_back(readPrimitive(*entries[primitive], Diagnostics::item("primitive", primitive, owner)));
        }

        if (find(object, "weights") != nullptr) {
            mesh.weights = readFloats(object, "weights", owner, std::nullopt);
        }

        mesh.extras = readExtras(object);
        mesh.extensions = readExtensions(object, owner);
        document.meshes.push_back(std::move(mesh));
    }
}

void DocumentParser::readNodeTransform(const core::Json& object, const std::string& owner, Node& node) {
    const bool trs = find(object, "translation") != nullptr || find(object, "rotation") != nullptr || find(object, "scale") != nullptr;
    if (find(object, "matrix") != nullptr) {
        if (trs) {
            diagnostics.invalid(Problem::NodeTransform, owner + " gives both a matrix and a translation, rotation or scale");
        }

        const std::vector<float> values = readFloats(object, "matrix", owner, 16);
        math::Mat4 matrix;
        std::ranges::copy(values, matrix.values.begin());
        node.matrix = matrix;

        // A matrix must split into translation, rotation and scale that compose back into it, so it holds no shear and no projection.
        const bool affine = matrix.values[3] == 0.0F && matrix.values[7] == 0.0F && matrix.values[11] == 0.0F && matrix.values[15] == 1.0F;
        std::optional<math::Mat4::Decomposition> parts;
        if (affine) {
            try {
                parts = matrix.decompose();
            } catch (const std::invalid_argument&) {
                parts.reset();
            }
        }

        bool decomposable = parts.has_value();
        if (parts) {
            const math::Mat4 composed = math::Mat4::compose(parts->translation, parts->rotation, parts->scale);
            float largest = 1.0F;
            float error = 0.0F;
            for (std::size_t element = 0; element < 12; ++element) {
                largest = std::max(largest, std::abs(matrix.values[element]));
                error = std::max(error, std::abs(matrix.values[element] - composed.values[element]));
            }

            decomposable = error <= 0.00005F * largest;
            node.translation = parts->translation;
            node.rotation = parts->rotation;
            node.scale = parts->scale;
        }

        if (!decomposable) {
            diagnostics.invalid(Problem::NodeTransform, "the matrix of " + owner + " does not split into translation, rotation and scale, since it shears, projects or scales an axis to zero");
        }

        return;
    }

    if (find(object, "translation") != nullptr) {
        const std::vector<float> values = readFloats(object, "translation", owner, 3);
        node.translation = {values[0], values[1], values[2]};
    }

    if (find(object, "rotation") != nullptr) {
        const std::vector<float> values = readFloats(object, "rotation", owner, 4);
        for (const float component : values) {
            requireRange(component, -1.0, 1.0, "rotation", owner);
        }

        node.rotation = {values[0], values[1], values[2], values[3]};
        const double length = std::sqrt(static_cast<double>(values[0]) * values[0] + static_cast<double>(values[1]) * values[1] + static_cast<double>(values[2]) * values[2] + static_cast<double>(values[3]) * values[3]);
        if (std::abs(length - 1.0) > FormatNames::kUnitLengthThresholdVec4) {
            diagnostics.invalid(Problem::NotUnit, "the rotation of " + owner + " is not a unit quaternion");
        }
    }

    if (find(object, "scale") != nullptr) {
        const std::vector<float> values = readFloats(object, "scale", owner, 3);
        node.scale = {values[0], values[1], values[2]};
    }
}

void DocumentParser::readNodes() {
    const std::vector<const core::Json*> items = readList(root, "nodes", "node", "the document", false);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const core::Json& object = *items[index];
        const std::string owner = Diagnostics::item("node", index);
        Node node;
        node.name = readOptionalString(object, "name", owner).value_or("");
        node.children = readIndices(object, "children", owner, counts.nodes, "node");
        node.camera = readOptionalIndex(object, "camera", owner, counts.cameras, "camera");
        node.mesh = readOptionalIndex(object, "mesh", owner, counts.meshes, "mesh");
        node.skin = readOptionalIndex(object, "skin", owner, counts.skins, "skin");
        if (node.skin && !node.mesh) {
            diagnostics.invalid(Problem::SkinMesh, owner + " has a skin without a mesh");
        }

        if (find(object, "weights") != nullptr) {
            if (!node.mesh) {
                diagnostics.invalid(Problem::WeightsCount, owner + " has morph weights without a mesh");
            }

            node.weights = readFloats(object, "weights", owner, std::nullopt);
        }

        readNodeTransform(object, owner, node);
        node.extras = readExtras(object);
        node.extensions = readExtensions(object, owner);
        document.nodes.push_back(std::move(node));
    }
}

void DocumentParser::readSkins() {
    const std::vector<const core::Json*> items = readList(root, "skins", "skin", "the document", false);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const core::Json& object = *items[index];
        const std::string owner = Diagnostics::item("skin", index);
        Skin skin;
        skin.name = readOptionalString(object, "name", owner).value_or("");
        skin.inverseBindMatrices = readOptionalIndex(object, "inverseBindMatrices", owner, counts.accessors, "accessor");
        skin.skeleton = readOptionalIndex(object, "skeleton", owner, counts.nodes, "node");
        if (find(object, "joints") == nullptr) {
            diagnostics.invalid(Problem::MissingProperty, owner + " lacks the key \"joints\"");
        }

        skin.joints = readIndices(object, "joints", owner, counts.nodes, "node");

        skin.extras = readExtras(object);
        skin.extensions = readExtensions(object, owner);
        document.skins.push_back(std::move(skin));
    }
}

void DocumentParser::readCameras() {
    const std::vector<const core::Json*> items = readList(root, "cameras", "camera", "the document", false);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const core::Json& object = *items[index];
        const std::string owner = Diagnostics::item("camera", index);
        Camera camera;
        camera.name = readOptionalString(object, "name", owner).value_or("");
        const std::string type = readString(object, "type", owner);
        const std::optional<Camera::Type> found = FormatNames::findCameraType(type);
        if (!found) {
            diagnostics.invalid(Problem::InvalidValue, Diagnostics::key("type", owner) + " holds \"" + type + "\", which is neither \"perspective\" nor \"orthographic\"");
        }

        camera.type = *found;
        // clang-format off
        const auto positive = [&](const core::Json& holder, std::string_view key, const std::string& holderName) {
            const float value = readFloat(require(holder, key, holderName), key, holderName);
            if (!(value > 0.0F)) {
                diagnostics.invalid(Problem::InvalidCamera, Diagnostics::key(key, holderName) + " is not greater than zero");
            }

            return value;
        };
        // clang-format on

        if (camera.type == Camera::Type::Perspective) {
            const std::string projection = Diagnostics::key("perspective", owner);
            const core::Json* perspective = findObject(object, "perspective", owner);
            if (perspective == nullptr) {
                diagnostics.invalid(Problem::MissingProperty, owner + " is a perspective camera without the key \"perspective\"");
            }

            if (find(*perspective, "aspectRatio") != nullptr) {
                camera.perspective.aspectRatio = positive(*perspective, "aspectRatio", projection);
            }

            camera.perspective.yfov = positive(*perspective, "yfov", projection);
            camera.perspective.znear = positive(*perspective, "znear", projection);
            if (find(*perspective, "zfar") != nullptr) {
                camera.perspective.zfar = positive(*perspective, "zfar", projection);
                if (*camera.perspective.zfar <= camera.perspective.znear) {
                    diagnostics.invalid(Problem::InvalidCamera, "the far plane of " + owner + " is not farther than its near plane");
                }
            }

            (void)readExtensions(*perspective, projection);
        } else {
            const std::string projection = Diagnostics::key("orthographic", owner);
            const core::Json* orthographic = findObject(object, "orthographic", owner);
            if (orthographic == nullptr) {
                diagnostics.invalid(Problem::MissingProperty, owner + " is an orthographic camera without the key \"orthographic\"");
            }

            camera.orthographic.xmag = readFloat(require(*orthographic, "xmag", projection), "xmag", projection);
            camera.orthographic.ymag = readFloat(require(*orthographic, "ymag", projection), "ymag", projection);
            camera.orthographic.znear = readFloat(require(*orthographic, "znear", projection), "znear", projection);
            camera.orthographic.zfar = positive(*orthographic, "zfar", projection);
            if (camera.orthographic.xmag == 0.0F || camera.orthographic.ymag == 0.0F) {
                diagnostics.invalid(Problem::InvalidCamera, "the view of " + owner + " has a width or a height of zero");
            }

            if (camera.orthographic.znear < 0.0F || camera.orthographic.zfar <= camera.orthographic.znear) {
                diagnostics.invalid(Problem::InvalidCamera, "the near plane of " + owner + " is negative or not nearer than its far plane");
            }

            (void)readExtensions(*orthographic, projection);
        }

        camera.extras = readExtras(object);
        camera.extensions = readExtensions(object, owner);
        document.cameras.push_back(std::move(camera));
    }
}

Animation DocumentParser::readAnimation(const core::Json& object, const std::string& owner) {
    Animation animation;
    animation.name = readOptionalString(object, "name", owner).value_or("");
    const std::vector<const core::Json*> samplers = readList(object, "samplers", "sampler", owner, true);
    for (std::size_t index = 0; index < samplers.size(); ++index) {
        const core::Json& entry = *samplers[index];
        const std::string samplerOwner = Diagnostics::item("sampler", index, owner);
        Animation::Sampler sampler;
        sampler.input = readIndex(entry, "input", samplerOwner, counts.accessors, "accessor");
        sampler.output = readIndex(entry, "output", samplerOwner, counts.accessors, "accessor");
        if (const std::optional<std::string> interpolation = readOptionalString(entry, "interpolation", samplerOwner)) {
            const std::optional<Animation::Interpolation> found = FormatNames::findInterpolation(*interpolation);
            if (!found) {
                diagnostics.invalid(Problem::InvalidValue, Diagnostics::key("interpolation", samplerOwner) + " holds \"" + *interpolation + "\", which is none of \"STEP\", \"LINEAR\" and \"CUBICSPLINE\"");
            }

            sampler.interpolation = *found;
        }

        sampler.extras = readExtras(entry);
        sampler.extensions = readExtensions(entry, samplerOwner);
        animation.samplers.push_back(std::move(sampler));
    }

    const std::vector<const core::Json*> channels = readList(object, "channels", "channel", owner, true);
    for (std::size_t index = 0; index < channels.size(); ++index) {
        const core::Json& entry = *channels[index];
        const std::string channelOwner = Diagnostics::item("channel", index, owner);
        Animation::Channel channel;
        channel.sampler = readIndex(entry, "sampler", channelOwner, animation.samplers.size(), "sampler");
        const std::string targetOwner = Diagnostics::key("target", channelOwner);
        const core::Json* target = findObject(entry, "target", channelOwner);
        if (target == nullptr) {
            diagnostics.invalid(Problem::MissingProperty, channelOwner + " lacks the key \"target\"");
        }

        channel.node = readOptionalIndex(*target, "node", targetOwner, counts.nodes, "node");
        const std::string path = readString(*target, "path", targetOwner);
        const std::optional<Animation::Path> found = FormatNames::findPath(path);
        channel.targetExtras = readExtras(*target);
        channel.targetExtensions = readExtensions(*target, targetOwner);
        channel.extras = readExtras(entry);
        channel.extensions = readExtensions(entry, channelOwner);

        // A path that glTF 2.0 does not define belongs to an extension, whose channel stays out of the channels of the core.
        if (!found) {
            continue;
        }

        channel.path = *found;
        animation.channels.push_back(std::move(channel));
    }

    animation.extras = readExtras(object);
    animation.extensions = readExtensions(object, owner);
    return animation;
}

void DocumentParser::readAnimations() {
    const std::vector<const core::Json*> items = readList(root, "animations", "animation", "the document", false);
    for (std::size_t index = 0; index < items.size(); ++index) {
        document.animations.push_back(readAnimation(*items[index], Diagnostics::item("animation", index)));
    }
}

void DocumentParser::readScenes() {
    const std::vector<const core::Json*> items = readList(root, "scenes", "scene", "the document", false);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const core::Json& object = *items[index];
        const std::string owner = Diagnostics::item("scene", index);
        Scene scene;
        scene.name = readOptionalString(object, "name", owner).value_or("");
        scene.nodes = readIndices(object, "nodes", owner, counts.nodes, "node");
        scene.extras = readExtras(object);
        scene.extensions = readExtensions(object, owner);
        document.scenes.push_back(std::move(scene));
    }
}

std::vector<const core::Json*> DocumentParser::readList(const core::Json& owner, std::string_view key, std::string_view itemKind, const std::string& ownerName, bool required) {
    std::vector<const core::Json*> items;
    const core::Json* list = required ? &require(owner, key, ownerName) : find(owner, key);
    if (list == nullptr) {
        return items;
    }

    if (!list->is_array()) {
        wrongType(key, ownerName, "an array");
    }

    if (list->empty()) {
        diagnostics.invalid(Problem::EmptyElement, Diagnostics::key(key, ownerName) + " is an empty list");
    }

    countObjects(list->size());
    items.reserve(list->size());
    for (std::size_t index = 0; index < list->size(); ++index) {
        const core::Json& item = list->at(index);
        if (!item.is_object()) {
            diagnostics.invalid(Problem::WrongType, Diagnostics::item(itemKind, index) + " of " + ownerName + " holds " + std::string(describeType(item)) + " where an object belongs");
        }

        items.push_back(&item);
    }

    return items;
}

void DocumentParser::countObjects(std::size_t count) {
    if (count > limits.maxObjects - std::min(objects, limits.maxObjects)) {
        diagnostics.refuse(Problem::LimitExceeded, "holds more than the " + std::to_string(limits.maxObjects) + " objects the reader takes from one file", "Split the model into smaller files.");
    }

    objects += count;
}

const core::Json* DocumentParser::find(const core::Json& object, std::string_view key) const {
    const auto found = object.find(key);
    return found == object.end() ? nullptr : &*found;
}

const core::Json& DocumentParser::require(const core::Json& object, std::string_view key, const std::string& owner) const {
    const core::Json* value = find(object, key);
    if (value == nullptr) {
        diagnostics.invalid(Problem::MissingProperty, owner + " lacks the key \"" + std::string(key) + "\"");
    }

    return *value;
}

const core::Json* DocumentParser::findObject(const core::Json& object, std::string_view key, const std::string& owner) const {
    const core::Json* value = find(object, key);
    if (value != nullptr && !value->is_object()) {
        wrongType(key, owner, "an object");
    }

    return value;
}

std::string DocumentParser::readString(const core::Json& object, std::string_view key, const std::string& owner) const {
    const core::Json& value = require(object, key, owner);
    if (!value.is_string()) {
        wrongType(key, owner, "a string");
    }

    return value.get<std::string>();
}

std::optional<std::string> DocumentParser::readOptionalString(const core::Json& object, std::string_view key, const std::string& owner) const {
    if (find(object, key) == nullptr) {
        return std::nullopt;
    }

    return readString(object, key, owner);
}

bool DocumentParser::readBool(const core::Json& object, std::string_view key, const std::string& owner) const {
    const core::Json* value = find(object, key);
    if (value == nullptr) {
        return false;
    }

    if (!value->is_boolean()) {
        wrongType(key, owner, "true or false");
    }

    return value->get<bool>();
}

std::uint64_t DocumentParser::readInteger(const core::Json& value, std::string_view key, const std::string& owner, std::uint64_t minimum, std::uint64_t maximum) const {
    std::uint64_t number = 0;
    if (value.is_number_unsigned()) {
        number = value.get<std::uint64_t>();
    } else if (value.is_number_integer()) {
        if (value.get<std::int64_t>() < 0) {
            diagnostics.invalid(Problem::InvalidValue, Diagnostics::key(key, owner) + " is negative");
        }

        number = static_cast<std::uint64_t>(value.get<std::int64_t>());
    } else if (value.is_number_float()) {
        // A whole number may be written with a fraction of zero or an exponent.
        const double real = value.get<double>();
        if (!std::isfinite(real) || real != std::floor(real) || real < 0.0 || real >= 18446744073709551616.0) {
            diagnostics.invalid(Problem::WrongType, Diagnostics::key(key, owner) + " holds " + value.dump() + ", which is not a whole number that is not negative");
        }

        number = static_cast<std::uint64_t>(real);
    } else {
        wrongType(key, owner, "a whole number");
    }

    if (number < minimum || number > maximum) {
        diagnostics.invalid(Problem::InvalidValue, Diagnostics::key(key, owner) + " holds " + std::to_string(number) + ", outside the range from " + std::to_string(minimum) + " to " + std::to_string(maximum));
    }

    return number;
}

std::optional<std::uint64_t> DocumentParser::readOptionalInteger(const core::Json& object, std::string_view key, const std::string& owner, std::uint64_t minimum, std::uint64_t maximum) const {
    const core::Json* value = find(object, key);
    if (value == nullptr) {
        return std::nullopt;
    }

    return readInteger(*value, key, owner, minimum, maximum);
}

std::size_t DocumentParser::readIndex(const core::Json& object, std::string_view key, const std::string& owner, std::size_t count, std::string_view kind) const {
    const std::uint64_t index = readInteger(require(object, key, owner), key, owner, 0, std::numeric_limits<std::uint64_t>::max());
    requireIndex(index, key, owner, count, kind);
    return static_cast<std::size_t>(index);
}

void DocumentParser::requireIndex(std::uint64_t index, std::string_view key, const std::string& owner, std::size_t count, std::string_view kind) const {
    if (index >= count) {
        diagnostics.invalid(Problem::InvalidIndex, Diagnostics::key(key, owner) + " names " + std::string(kind) + " " + std::to_string(index) + ", which the file lacks");
    }
}

std::optional<std::size_t> DocumentParser::readOptionalIndex(const core::Json& object, std::string_view key, const std::string& owner, std::size_t count, std::string_view kind) const {
    if (find(object, key) == nullptr) {
        return std::nullopt;
    }

    return readIndex(object, key, owner, count, kind);
}

std::vector<std::size_t> DocumentParser::readIndices(const core::Json& object, std::string_view key, const std::string& owner, std::size_t count, std::string_view kind) {
    std::vector<std::size_t> indices;
    const core::Json* list = find(object, key);
    if (list == nullptr) {
        return indices;
    }

    if (!list->is_array()) {
        wrongType(key, owner, "an array of indices");
    }

    if (list->empty()) {
        diagnostics.invalid(Problem::EmptyElement, Diagnostics::key(key, owner) + " is an empty list");
    }

    countObjects(list->size());
    std::set<std::size_t> seen;
    for (const core::Json& entry : *list) {
        const std::uint64_t index = readInteger(entry, key, owner, 0, std::numeric_limits<std::uint64_t>::max());
        requireIndex(index, key, owner, count, kind);

        if (!seen.insert(static_cast<std::size_t>(index)).second) {
            diagnostics.invalid(Problem::DuplicateElement, Diagnostics::key(key, owner) + " names " + std::string(kind) + " " + std::to_string(index) + " twice");
        }

        indices.push_back(static_cast<std::size_t>(index));
    }

    return indices;
}

double DocumentParser::readNumber(const core::Json& value, std::string_view key, const std::string& owner) const {
    if (!value.is_number()) {
        wrongType(key, owner, "a number");
    }

    const double number = value.get<double>();
    if (!std::isfinite(number)) {
        diagnostics.invalid(Problem::InvalidValue, Diagnostics::key(key, owner) + " holds a number that is not finite");
    }

    return number;
}

float DocumentParser::readFloat(const core::Json& value, std::string_view key, const std::string& owner) const {
    const double number = readNumber(value, key, owner);
    if (std::abs(number) > static_cast<double>(std::numeric_limits<float>::max())) {
        diagnostics.invalid(Problem::InvalidValue, Diagnostics::key(key, owner) + " holds the number " + value.dump() + ", which lies outside the numbers from -3.4028234663853e+38 to 3.4028234663853e+38 that the engine holds");
    }

    return static_cast<float>(number);
}

std::optional<float> DocumentParser::readOptionalFloat(const core::Json& object, std::string_view key, const std::string& owner) const {
    const core::Json* value = find(object, key);
    if (value == nullptr) {
        return std::nullopt;
    }

    return readFloat(*value, key, owner);
}

std::vector<float> DocumentParser::readFloats(const core::Json& object, std::string_view key, const std::string& owner, std::optional<std::size_t> size) {
    const core::Json& list = require(object, key, owner);
    if (!list.is_array() || (size && list.size() != *size) || list.empty()) {
        diagnostics.invalid(Problem::WrongType, Diagnostics::key(key, owner) + " does not hold " + (size ? std::to_string(*size) + " numbers" : std::string("a list of numbers")));
    }

    countObjects(list.size());
    std::vector<float> values;
    values.reserve(list.size());
    for (const core::Json& entry : list) {
        values.push_back(readFloat(entry, key, owner));
    }

    return values;
}

core::Json DocumentParser::readExtras(const core::Json& object) const {
    const core::Json* extras = find(object, "extras");
    return extras == nullptr ? core::Json() : *extras;
}

core::Json DocumentParser::readExtensions(const core::Json& object, const std::string& owner) const {
    const core::Json* extensions = find(object, "extensions");
    if (extensions == nullptr) {
        return core::Json();
    }

    if (!extensions->is_object()) {
        wrongType("extensions", owner, "an object");
    }

    for (const auto& [name, value] : extensions->items()) {
        if (!isUsed(name)) {
            diagnostics.invalid(Problem::UndeclaredExtension, owner + " uses the extension \"" + name + "\", which the list \"extensionsUsed\" of the file lacks");
        }
    }

    return *extensions;
}

void DocumentParser::requireRange(double value, double minimum, double maximum, std::string_view key, const std::string& owner) const {
    if (value < minimum || value > maximum) {
        diagnostics.invalid(Problem::InvalidValue, Diagnostics::key(key, owner) + " holds " + core::Json(value).dump() + ", outside the range from " + core::Json(minimum).dump() + " to " + core::Json(maximum).dump());
    }
}

void DocumentParser::wrongType(std::string_view key, const std::string& owner, std::string_view expected) const {
    diagnostics.invalid(Problem::WrongType, Diagnostics::key(key, owner) + " does not hold " + std::string(expected));
}

std::string_view DocumentParser::describeType(const core::Json& value) noexcept {
    if (value.is_object()) {
        return "an object";
    }

    if (value.is_array()) {
        return "an array";
    }

    if (value.is_string()) {
        return "a string";
    }

    if (value.is_number()) {
        return "a number";
    }

    if (value.is_boolean()) {
        return "true or false";
    }

    return "null";
}

bool DocumentParser::isUsed(std::string_view extension) const noexcept {
    return std::ranges::find(document.extensionsUsed, extension) != document.extensionsUsed.end();
}

} // namespace haylen::gltf
