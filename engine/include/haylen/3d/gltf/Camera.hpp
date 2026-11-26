#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "haylen/3d/gltf/NameTable.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::gltf {

// A camera that a node places, which looks along negative `z` of its node with `y` up. Distances are in meters and the field of view in radians.
struct Camera {
    enum class Type : std::uint8_t {
        Perspective,
        Orthographic,
    };

    // A perspective without a far plane reaches infinitely far, and one without an aspect ratio takes the ratio of what it draws into.
    struct Perspective {
        std::optional<float> aspectRatio;
        float yfov = 0.0F;
        std::optional<float> zfar;
        float znear = 0.0F;
    };

    // The half width and half height of the view.
    struct Orthographic {
        float xmag = 0.0F;
        float ymag = 0.0F;
        float zfar = 0.0F;
        float znear = 0.0F;
    };

    std::string name;
    Type type = Type::Perspective;
    Perspective perspective;
    Orthographic orthographic;
    core::Json extras;
    core::Json extensions;
};

} // namespace haylen::gltf

namespace haylen::core {

template <> struct EnumNames<haylen::gltf::Camera::Type> {
    using Value = haylen::gltf::Camera::Type;

    static constexpr haylen::gltf::NameTable::Entries<Value, 2> kNames{{{"perspective", Value::Perspective}, {"orthographic", Value::Orthographic}}};

    static std::optional<Value> fromName(std::string_view name) {
        return haylen::gltf::NameTable::find(kNames, name);
    }
    static std::string_view name(Value value) {
        return haylen::gltf::NameTable::name(kNames, value);
    }
};

} // namespace haylen::core
