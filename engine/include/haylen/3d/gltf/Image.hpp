#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "haylen/3d/gltf/Buffer.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::gltf {

// An image that textures sample, held encoded as the file stores it, in a buffer view, a file beside the document or a `data:` URI, and decoded only when it is asked for.
struct Image {
    std::string name;
    Buffer::Storage storage = Buffer::Storage::BufferView;
    std::optional<std::size_t> bufferView;

    // The URI as the file writes it and the path of the image in the content folder of the package, for an image in a file beside the document.
    std::string uri;
    std::string path;

    // The media type of the encoded bytes, such as `image/png`, as the file names it or as the signature of the bytes tells.
    std::string mimeType;

    // The encoded bytes of an image in a file or a `data:` URI, which the reader loaded. An image in a buffer view reads its bytes from its buffer.
    std::vector<std::uint8_t> data;
    core::Json extras;
    core::Json extensions;
};

} // namespace haylen::gltf
