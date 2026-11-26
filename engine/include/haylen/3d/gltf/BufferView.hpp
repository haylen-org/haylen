#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "haylen/3d/gltf/NameTable.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::gltf {

// A range of bytes of a buffer, which accessors read their elements from and images their encoded bytes.
struct BufferView {
    // What the GPU binds the bytes as, which a file may name for the vertex and the index data.
    enum class Target : std::uint8_t {
        ArrayBuffer,
        ElementArrayBuffer,
    };

    std::string name;
    std::size_t buffer = 0;
    std::uint64_t byteOffset = 0;
    std::uint64_t byteLength = 0;
    std::optional<std::uint32_t> byteStride;
    std::optional<Target> target;
    core::Json extras;
    core::Json extensions;
};

} // namespace haylen::gltf

namespace haylen::core {

template <> struct EnumNames<haylen::gltf::BufferView::Target> {
    using Value = haylen::gltf::BufferView::Target;

    static constexpr haylen::gltf::NameTable::Entries<Value, 2> kNames{{{"arrayBuffer", Value::ArrayBuffer}, {"elementArrayBuffer", Value::ElementArrayBuffer}}};

    static std::optional<Value> fromName(std::string_view name) {
        return haylen::gltf::NameTable::find(kNames, name);
    }
    static std::string_view name(Value value) {
        return haylen::gltf::NameTable::name(kNames, value);
    }
};

} // namespace haylen::core
