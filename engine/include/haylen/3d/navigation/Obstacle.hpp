#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/core/EnumNames.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::navigation3d {

// A shape that carves the mesh while it stands, such as a crate, a car or a closed gate. A cylinder stands on `position` with its `radius` and `height`, and a box stands around `position` with its `halfExtents`, turned by `yaw` radians around the up axis. The mesh keeps every agent its radius away from an obstacle.
struct Obstacle {
    enum class Shape : std::uint8_t {
        Cylinder,
        Box,
    };

    Shape shape = Shape::Cylinder;
    math::Vec3 position;
    float radius = 0.5F;
    float height = 2.0F;
    math::Vec3 halfExtents{0.5F, 1.0F, 0.5F};
    float yaw = 0.0F;

    [[nodiscard]] static Obstacle cylinder(math::Vec3 base, float radius, float height) noexcept {
        return {.shape = Shape::Cylinder, .position = base, .radius = radius, .height = height};
    }
    [[nodiscard]] static Obstacle box(math::Vec3 center, math::Vec3 halfExtents, float yaw = 0.0F) noexcept {
        return {.shape = Shape::Box, .position = center, .halfExtents = halfExtents, .yaw = yaw};
    }

    friend bool operator==(const Obstacle&, const Obstacle&) = default;
};

} // namespace haylen::navigation3d

namespace haylen::core {

template <> struct EnumNames<navigation3d::Obstacle::Shape> {
    using Shape = navigation3d::Obstacle::Shape;

    static constexpr std::array<std::pair<std::string_view, Shape>, 2> kNames{{{"cylinder", Shape::Cylinder}, {"box", Shape::Box}}};

    static std::optional<Shape> fromName(std::string_view name) {
        const auto found = std::ranges::find(kNames, name, &std::pair<std::string_view, Shape>::first);
        return found != kNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(Shape value) {
        return std::ranges::find(kNames, value, &std::pair<std::string_view, Shape>::second)->first;
    }
};

} // namespace haylen::core
