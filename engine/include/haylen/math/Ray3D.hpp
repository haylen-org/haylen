#pragma once

#include <limits>

#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A half-line in 3D that starts at the origin, runs along a unit direction and stops after its length. Casts measure their hits as distances from the origin.
struct Ray3D {
    Vec3 origin;
    Vec3 direction = Vec3::forward();
    float length = std::numeric_limits<float>::infinity();

    // Returns the ray from one point to the other, which points along `Vec3::forward()` with zero length when both points are equal.
    [[nodiscard]] static Ray3D between(Vec3 from, Vec3 to) noexcept {
        const Vec3 offset = to - from;
        const float distance = offset.getLength();
        return {from, distance > 0.0F ? offset / distance : Vec3::forward(), distance};
    }

    [[nodiscard]] constexpr Vec3 at(float distance) const noexcept {
        return origin + direction * distance;
    }
    [[nodiscard]] constexpr Vec3 getEndPoint() const noexcept {
        return at(length);
    }
};

} // namespace haylen::math
