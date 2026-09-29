#pragma once

#include <limits>

#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// A half-line that starts at the origin, runs along a unit direction and stops after its length. Casts measure their hits as distances from the origin.
struct Ray {
    Vec2 origin{};
    Vec2 direction{1.0F, 0.0F};
    float length = std::numeric_limits<float>::infinity();

    // Returns the ray from one point to the other, which points along x with zero length when both points are equal.
    [[nodiscard]] static Ray between(Vec2 from, Vec2 to) noexcept {
        const Vec2 offset = to - from;
        const float distance = offset.getLength();
        return {from, distance > 0.0F ? offset / distance : Vec2{1.0F, 0.0F}, distance};
    }

    [[nodiscard]] static Ray fromAngle(Vec2 start, float radians, float distance) noexcept {
        return {start, Vec2::fromAngle(radians), distance};
    }

    [[nodiscard]] constexpr Vec2 at(float distance) const noexcept {
        return origin + direction * distance;
    }
    [[nodiscard]] constexpr Vec2 getEnd() const noexcept {
        return at(length);
    }
};

} // namespace haylen::math
