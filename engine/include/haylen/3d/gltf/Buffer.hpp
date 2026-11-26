#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/gltf/NameTable.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::gltf {

// The bytes of geometry, animation and images that buffer views divide, read from the binary chunk of a GLB file, a file beside a `.gltf` file or a `data:` URI.
struct Buffer {
    // Where the bytes of a buffer or an image come from.
    enum class Storage : std::uint8_t {
        Glb,
        File,
        DataUri,
        BufferView,
    };

    std::string name;
    Storage storage = Storage::Glb;

    // The URI as the file writes it and the path of the file in the content folder of the package, for a buffer in a file beside the document.
    std::string uri;
    std::string path;

    std::uint64_t byteLength = 0;

    // The first `byteLength` bytes of the resource, which the reader loaded.
    std::vector<std::uint8_t> data;
    core::Json extras;
    core::Json extensions;
};

} // namespace haylen::gltf

namespace haylen::core {

template <> struct EnumNames<haylen::gltf::Buffer::Storage> {
    using Value = haylen::gltf::Buffer::Storage;

    static constexpr haylen::gltf::NameTable::Entries<Value, 4> kNames{{{"glb", Value::Glb}, {"file", Value::File}, {"dataUri", Value::DataUri}, {"bufferView", Value::BufferView}}};

    static std::optional<Value> fromName(std::string_view name) {
        return haylen::gltf::NameTable::find(kNames, name);
    }
    static std::string_view name(Value value) {
        return haylen::gltf::NameTable::name(kNames, value);
    }
};

} // namespace haylen::core
