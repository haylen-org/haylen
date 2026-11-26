#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "haylen/3d/gltf/NameTable.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::gltf {

// How a texture filters its texels when it grows and shrinks on the screen and how it repeats along each axis. A filter that the file leaves out is the choice of the renderer.
struct Sampler {
    enum class Filter : std::uint8_t {
        Nearest,
        Linear,
        NearestMipmapNearest,
        LinearMipmapNearest,
        NearestMipmapLinear,
        LinearMipmapLinear,
    };

    enum class Wrap : std::uint8_t {
        ClampToEdge,
        MirroredRepeat,
        Repeat,
    };

    std::string name;

    // The filter of a texture that grows, which is only `Nearest` or `Linear`, and of a texture that shrinks.
    std::optional<Filter> magFilter;
    std::optional<Filter> minFilter;
    Wrap wrapS = Wrap::Repeat;
    Wrap wrapT = Wrap::Repeat;
    core::Json extras;
    core::Json extensions;
};

} // namespace haylen::gltf

namespace haylen::core {

template <> struct EnumNames<haylen::gltf::Sampler::Filter> {
    using Value = haylen::gltf::Sampler::Filter;

    static constexpr haylen::gltf::NameTable::Entries<Value, 6> kNames{{{"nearest", Value::Nearest}, {"linear", Value::Linear}, {"nearestMipmapNearest", Value::NearestMipmapNearest}, {"linearMipmapNearest", Value::LinearMipmapNearest}, {"nearestMipmapLinear", Value::NearestMipmapLinear}, {"linearMipmapLinear", Value::LinearMipmapLinear}}};

    static std::optional<Value> fromName(std::string_view name) {
        return haylen::gltf::NameTable::find(kNames, name);
    }
    static std::string_view name(Value value) {
        return haylen::gltf::NameTable::name(kNames, value);
    }
};

template <> struct EnumNames<haylen::gltf::Sampler::Wrap> {
    using Value = haylen::gltf::Sampler::Wrap;

    static constexpr haylen::gltf::NameTable::Entries<Value, 3> kNames{{{"clampToEdge", Value::ClampToEdge}, {"mirroredRepeat", Value::MirroredRepeat}, {"repeat", Value::Repeat}}};

    static std::optional<Value> fromName(std::string_view name) {
        return haylen::gltf::NameTable::find(kNames, name);
    }
    static std::string_view name(Value value) {
        return haylen::gltf::NameTable::name(kNames, value);
    }
};

} // namespace haylen::core
