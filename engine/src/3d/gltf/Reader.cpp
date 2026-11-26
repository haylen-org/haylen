#include "haylen/3d/gltf/Reader.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

#include "3d/gltf/DataValidator.hpp"
#include "3d/gltf/Diagnostics.hpp"
#include "3d/gltf/DocumentParser.hpp"
#include "3d/gltf/GlbContainer.hpp"
#include "3d/gltf/LinkValidator.hpp"
#include "3d/gltf/UriResolver.hpp"
#include "haylen/core/JsonValidator.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/PackageReader.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::gltf {

using Problem = ReadError::Problem;

Reader::Reader(const io::Package* source, Limits value) : package(source), limits(value) {}

Reader::Reader(Reader&&) noexcept = default;
Reader& Reader::operator=(Reader&&) noexcept = default;
Reader::~Reader() = default;

Reader Reader::open(const io::Package& package, std::string_view path, Limits limits) {
    Reader reader(&package, limits);
    reader.document.file = std::string(path);
    const Diagnostics diagnostics(reader.document.file);
    if (!package.assetExists(path)) {
        diagnostics.refuse(Problem::MissingFile, "is not in the content folder of the package", "Check the path of the model, which is relative to the content folder.");
    }

    reader.file = package.openAssetReader(path);
    const std::uint64_t size = reader.file->getSize();
    std::array<std::uint8_t, 4> head{};
    const std::size_t headSize = reader.file->read(0, head);
    if (GlbContainer::isGlb(std::span(head.data(), headSize))) {
        // clang-format off
        const GlbContainer::Layout layout = GlbContainer::parse(size, [&reader](std::uint64_t offset, std::span<std::uint8_t> target) {
            reader.file->readExactly(offset, target);
        }, limits.maxJsonBytes, diagnostics);
        // clang-format on
        std::string json(static_cast<std::size_t>(layout.json.length), '\0');
        reader.file->readExactly(layout.json.offset, std::span(reinterpret_cast<std::uint8_t*>(json.data()), json.size()));
        reader.hasBinary = layout.binary.has_value();
        if (layout.binary) {
            reader.binaryOffset = layout.binary->offset;
            reader.binaryLength = layout.binary->length;
        }

        reader.parseDocument(json, true, true);
        return reader;
    }

    if (size > limits.maxJsonBytes) {
        diagnostics.refuse(Problem::LimitExceeded, "holds " + std::to_string(size) + " bytes of JSON, more than the " + std::to_string(limits.maxJsonBytes) + " bytes the reader takes", "Save the model as a GLB file, or keep its buffers in files beside it.");
    }

    std::string json(static_cast<std::size_t>(size), '\0');
    reader.file->readExactly(0, std::span(reinterpret_cast<std::uint8_t*>(json.data()), json.size()));
    reader.file.reset();
    reader.parseDocument(json, false, true);
    return reader;
}

Reader Reader::open(std::span<const std::uint8_t> bytes, std::string_view name, Limits limits) {
    Reader reader(nullptr, limits);
    reader.document.file = std::string(name);
    const Diagnostics diagnostics(reader.document.file);
    if (GlbContainer::isGlb(bytes)) {
        // clang-format off
        const GlbContainer::Layout layout = GlbContainer::parse(bytes.size(), [bytes](std::uint64_t offset, std::span<std::uint8_t> target) {
            std::ranges::copy(bytes.subspan(static_cast<std::size_t>(offset), target.size()), target.begin());
        }, limits.maxJsonBytes, diagnostics);
        // clang-format on
        const std::span<const std::uint8_t> json = bytes.subspan(static_cast<std::size_t>(layout.json.offset), static_cast<std::size_t>(layout.json.length));
        reader.memory = bytes;
        reader.hasBinary = layout.binary.has_value();
        if (layout.binary) {
            reader.binaryOffset = layout.binary->offset;
            reader.binaryLength = layout.binary->length;
        }

        reader.parseDocument(std::string_view(reinterpret_cast<const char*>(json.data()), json.size()), true, false);
        return reader;
    }

    if (bytes.size() > limits.maxJsonBytes) {
        diagnostics.refuse(Problem::LimitExceeded, "holds " + std::to_string(bytes.size()) + " bytes of JSON, more than the " + std::to_string(limits.maxJsonBytes) + " bytes the reader takes", "Send the model as a GLB file.");
    }

    reader.parseDocument(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), false, false);
    return reader;
}

Document Reader::read(const io::Package& package, std::string_view path, Limits limits) {
    Reader reader = open(package, path, limits);
    reader.loadResources();
    return reader.finish();
}

Document Reader::read(std::span<const std::uint8_t> bytes, std::string_view name, Limits limits) {
    Reader reader = open(bytes, name, limits);
    reader.loadResources();
    return reader.finish();
}

void Reader::parseDocument(std::string_view text, bool glb, bool filesAllowed) {
    const Diagnostics diagnostics(document.file);

    // A text document starts with an object, after white space and the byte order mark that some tools write.
    std::string_view start = text;
    if (start.starts_with("\xEF\xBB\xBF")) {
        start.remove_prefix(3);
    }

    const std::size_t first = start.find_first_not_of(" \t\r\n");
    if (!glb && (first == std::string_view::npos || start[first] != '{')) {
        diagnostics.refuse(Problem::NotGltf, "is neither a GLB file nor the JSON text of a glTF file", "Name a \".glb\" or \".gltf\" file.");
    }

    core::Json root;
    try {
        root = core::JsonValidator::parse(text, document.file, limits.maxJsonDepth);
    } catch (const std::invalid_argument& error) {
        throw ReadError(Problem::InvalidJson, error.what());
    }

    const DocumentParser::Source source{
        .glb = glb,
        .binaryLength = hasBinary ? std::optional(binaryLength) : std::nullopt,
        .directory = std::string(io::Path::directory(document.file)),
        .filesAllowed = filesAllowed,
    };
    const Diagnostics named(std::move(document.file));
    document = DocumentParser::parse(root, source, limits, named);
    LinkValidator::validate(document, Diagnostics(document.file));
}

std::vector<std::string> Reader::getExternalFiles() const {
    std::vector<std::string> files;
    // clang-format off
    const auto add = [&files](Buffer::Storage storage, const std::string& path) {
        if (storage == Buffer::Storage::File && std::ranges::find(files, path) == files.end()) {
            files.push_back(path);
        }
    };
    // clang-format on
    for (const Buffer& buffer : document.buffers) {
        add(buffer.storage, buffer.path);
    }

    for (const Image& image : document.images) {
        add(image.storage, image.path);
    }

    return files;
}

void Reader::loadResources() {
    if (loaded) {
        return;
    }

    for (std::size_t index = 0; index < document.buffers.size(); ++index) {
        loadBuffer(document.buffers[index], index);
    }

    for (std::size_t index = 0; index < document.images.size(); ++index) {
        loadImage(document.images[index], index);
    }

    file.reset();
    memory = {};
    loaded = true;
}

Document Reader::finish() {
    loadResources();
    DataValidator::validate(document, Diagnostics(document.file));
    return std::move(document);
}

void Reader::loadBuffer(Buffer& buffer, std::size_t index) {
    const Diagnostics diagnostics(document.file);
    const std::string owner = Diagnostics::item("buffer", index);
    switch (buffer.storage) {
    case Buffer::Storage::Glb:
        buffer.data.resize(static_cast<std::size_t>(buffer.byteLength));
        if (file) {
            file->readExactly(binaryOffset, buffer.data);
        } else {
            std::ranges::copy(memory.subspan(static_cast<std::size_t>(binaryOffset), buffer.data.size()), buffer.data.begin());
        }
        break;
    case Buffer::Storage::DataUri: {
        const UriResolver::DataUri data = UriResolver::parseDataUri(buffer.uri, owner, diagnostics);
        buffer.data = UriResolver::decodeDataUri(data, limits.maxDataUriBytes, owner, diagnostics);
        buffer.uri.clear();
        if (buffer.data.size() < buffer.byteLength) {
            diagnostics.invalid(Problem::ResourceTooShort, "the \"data:\" URI of " + owner + " holds " + std::to_string(buffer.data.size()) + " bytes, fewer than the " + std::to_string(buffer.byteLength) + " bytes of the buffer");
        }

        buffer.data.resize(static_cast<std::size_t>(buffer.byteLength));
        break;
    }
    case Buffer::Storage::File:
        buffer.data = readFile(buffer.path, owner, std::numeric_limits<std::uint64_t>::max(), buffer.byteLength);
        break;
    case Buffer::Storage::BufferView:
        break;
    }
}

void Reader::loadImage(Image& image, std::size_t index) {
    const Diagnostics diagnostics(document.file);
    const std::string owner = Diagnostics::item("image", index);
    if (image.storage == Buffer::Storage::DataUri) {
        const UriResolver::DataUri data = UriResolver::parseDataUri(image.uri, owner, diagnostics);
        image.data = UriResolver::decodeDataUri(data, limits.maxDataUriBytes, owner, diagnostics);
        image.uri.clear();
    } else if (image.storage == Buffer::Storage::File) {
        image.data = readFile(image.path, owner, limits.maxImageBytes, 0);
    }

    // The bytes of an image tell its format, which must be the format the file names for it.
    const std::string detected(detectMimeType(document.getImageBytes(index)));
    if (!detected.empty() && !image.mimeType.empty() && detected != image.mimeType) {
        diagnostics.invalid(Problem::InvalidImage, owner + " is named \"" + image.mimeType + "\" but holds an image of the type \"" + detected + "\"");
    }

    if (image.mimeType.empty()) {
        image.mimeType = detected;
    }
}

std::vector<std::uint8_t> Reader::readFile(const std::string& path, const std::string& owner, std::uint64_t maxBytes, std::uint64_t needed) {
    const Diagnostics diagnostics(document.file);
    if (!package->assetExists(path)) {
        diagnostics.refuse(Problem::MissingFile, "names the file \"" + path + "\" for " + owner + ", which the content folder of the package lacks", "Put the file beside the model, or export the model as a GLB file.");
    }

    const std::unique_ptr<io::PackageReader> reader = package->openAssetReader(path);
    const std::uint64_t size = reader->getSize();
    if (size < needed) {
        diagnostics.invalid(Problem::ResourceTooShort, "the file \"" + path + "\" of " + owner + " holds " + std::to_string(size) + " bytes, fewer than the " + std::to_string(needed) + " bytes of the buffer");
    }

    const std::uint64_t length = needed > 0 ? needed : size;
    if (length > maxBytes) {
        diagnostics.refuse(Problem::LimitExceeded, "names the file \"" + path + "\" of " + std::to_string(size) + " bytes for " + owner + ", more than the " + std::to_string(maxBytes) + " bytes the reader takes", "Make the image smaller.");
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    reader->readExactly(0, bytes);
    return bytes;
}

std::string_view Reader::detectMimeType(std::span<const std::uint8_t> bytes) noexcept {
    constexpr std::array<std::uint8_t, 8> kPng{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (bytes.size() >= kPng.size() && std::ranges::equal(bytes.first(kPng.size()), kPng)) {
        return "image/png";
    }

    if (bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
        return "image/jpeg";
    }

    if (holds(bytes, 0, "RIFF") && holds(bytes, 8, "WEBP")) {
        return "image/webp";
    }

    return {};
}

bool Reader::holds(std::span<const std::uint8_t> bytes, std::size_t offset, std::string_view text) noexcept {
    if (bytes.size() < offset + text.size()) {
        return false;
    }

    for (std::size_t index = 0; index < text.size(); ++index) {
        if (bytes[offset + index] != static_cast<std::uint8_t>(text[index])) {
            return false;
        }
    }

    return true;
}

} // namespace haylen::gltf
