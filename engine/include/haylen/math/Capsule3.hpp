#pragma once

#include "haylen/math/Box3.hpp"
#include "haylen/math/Segment3.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// The points within `radius` of the segment from `start` to `end`, such as the body of a character.
struct Capsule3 {
    Vec3 start;
    Vec3 end;
    float radius = 0.0F;

    [[nodiscard]] constexpr bool operator==(const Capsule3&) const noexcept = default;

    [[nodiscard]] constexpr Segment3 getAxis() const noexcept {
        return {start, end};
    }
    [[nodiscard]] constexpr bool contains(Vec3 point) const noexcept {
        return Vec3::distanceSquared(getAxis().getClosestPoint(point), point) <= radius * radius;
    }
    [[nodiscard]] constexpr Box3 getBounds() const noexcept {
        const Vec3 reach{radius, radius, radius};
        return {Vec3::min(start, end) - reach, Vec3::max(start, end) + reach};
    }
};

} // namespace haylen::math
