#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/gltf/NameTable.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::gltf {

// Keyframes that move the translation, rotation, scale or morph weights of nodes over time in seconds. Each channel connects one property of one node to a sampler, whose input accessor holds the times and whose output accessor holds the values.
struct Animation {
    // The property of a node that a channel animates.
    enum class Path : std::uint8_t {
        Translation,
        Rotation,
        Scale,
        Weights,
    };

    // How values between keyframes come: the value of the keyframe before, a straight line, or a cubic spline whose output holds an in tangent, a value and an out tangent for each keyframe.
    enum class Interpolation : std::uint8_t {
        Step,
        Linear,
        CubicSpline,
    };

    struct Sampler {
        std::size_t input = 0;
        std::size_t output = 0;
        Interpolation interpolation = Interpolation::Linear;
        core::Json extras;
        core::Json extensions;
    };

    // A channel without a node targets what an extension names, and the reader keeps it for that extension.
    struct Channel {
        std::size_t sampler = 0;
        std::optional<std::size_t> node;
        Path path = Path::Translation;
        core::Json extras;
        core::Json extensions;
        core::Json targetExtras;
        core::Json targetExtensions;
    };

    std::string name;
    std::vector<Channel> channels;
    std::vector<Sampler> samplers;
    core::Json extras;
    core::Json extensions;
};

} // namespace haylen::gltf

namespace haylen::core {

template <> struct EnumNames<haylen::gltf::Animation::Path> {
    using Value = haylen::gltf::Animation::Path;

    static constexpr haylen::gltf::NameTable::Entries<Value, 4> kNames{{{"translation", Value::Translation}, {"rotation", Value::Rotation}, {"scale", Value::Scale}, {"weights", Value::Weights}}};

    static std::optional<Value> fromName(std::string_view name) {
        return haylen::gltf::NameTable::find(kNames, name);
    }
    static std::string_view name(Value value) {
        return haylen::gltf::NameTable::name(kNames, value);
    }
};

template <> struct EnumNames<haylen::gltf::Animation::Interpolation> {
    using Value = haylen::gltf::Animation::Interpolation;

    static constexpr haylen::gltf::NameTable::Entries<Value, 3> kNames{{{"step", Value::Step}, {"linear", Value::Linear}, {"cubicSpline", Value::CubicSpline}}};

    static std::optional<Value> fromName(std::string_view name) {
        return haylen::gltf::NameTable::find(kNames, name);
    }
    static std::string_view name(Value value) {
        return haylen::gltf::NameTable::name(kNames, value);
    }
};

} // namespace haylen::core
