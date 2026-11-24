#pragma once

#include "haylen/math/Box.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

struct Sphere {
    Vec3 center;
    float radius = 0.0F;

    [[nodiscard]] constexpr bool operator==(const Sphere&) const noexcept = default;

    [[nodiscard]] constexpr bool contains(Vec3 point) const noexcept {
        return Vec3::distanceSquared(center, point) <= radius * radius;
    }
    [[nodiscard]] constexpr bool intersects(const Sphere& other) const noexcept {
        const float reach = radius + other.radius;
        return Vec3::distanceSquared(center, other.center) <= reach * reach;
    }
    [[nodiscard]] constexpr bool intersects(const Box& box) const noexcept {
        return contains(box.clamp(center));
    }
    [[nodiscard]] constexpr Box getBounds() const noexcept {
        return {center - Vec3{radius, radius, radius}, center + Vec3{radius, radius, radius}};
    }
};

} // namespace haylen::math
