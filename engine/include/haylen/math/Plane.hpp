#pragma once

#include <stdexcept>

#include "haylen/math/Vec3.hpp"
#include "haylen/math/Vec4.hpp"

namespace haylen::math {

// An infinite plane of the points `p` with `dot(normal, p) == distance`, whose normal has a length of one and points to its front side.
struct Plane {
    Vec3 normal = Vec3::up();
    float distance = 0.0F;

    // Returns the plane of `dot(facing, p) == offset`, scaling both so the normal has a length of one. Throws `std::invalid_argument` for a normal without a direction.
    [[nodiscard]] static Plane fromNormal(Vec3 facing, float offset) {
        const Vec3 direction = facing.getNormalized();
        const float length = Vec3::dot(facing, direction);
        return {direction, offset / length};
    }

    // Returns the plane through `point` whose normal points along `facing`. Throws `std::invalid_argument` for a normal without a direction.
    [[nodiscard]] static Plane fromPoint(Vec3 facing, Vec3 point) {
        const Vec3 direction = facing.getNormalized();
        return {direction, Vec3::dot(direction, point)};
    }

    // Returns the plane through the three points, whose front side sees them counter-clockwise. Throws `std::invalid_argument` for points on one line, which span no plane.
    [[nodiscard]] static Plane fromPoints(Vec3 a, Vec3 b, Vec3 c) {
        const Vec3 perpendicular = Vec3::cross(b - a, c - a);
        if (perpendicular.isZero() || !perpendicular.isFinite()) {
            throw std::invalid_argument("The three points lie on one line, so they span no plane.");
        }

        return fromPoint(perpendicular, a);
    }

    [[nodiscard]] constexpr bool operator==(const Plane&) const noexcept = default;

    // Returns how far the point lies in front of the plane, negative behind it.
    [[nodiscard]] constexpr float getSignedDistance(Vec3 point) const noexcept {
        return Vec3::dot(normal, point) - distance;
    }

    [[nodiscard]] constexpr Vec3 getClosestPoint(Vec3 point) const noexcept {
        return point - normal * getSignedDistance(point);
    }

    // Returns the four numbers `(a, b, c, d)` of the plane `a * x + b * y + c * z + d = 0`, which a matrix of planes takes.
    [[nodiscard]] constexpr Vec4 getCoefficients() const noexcept {
        return {normal.x, normal.y, normal.z, -distance};
    }
};

} // namespace haylen::math
