#pragma once

#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A triangle whose front side sees its corners run counter-clockwise from `a` to `b` to `c`.
struct Triangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;

    [[nodiscard]] constexpr bool operator==(const Triangle&) const noexcept = default;

    [[nodiscard]] float getArea() const noexcept {
        return Vec3::cross(b - a, c - a).getLength() * 0.5F;
    }

    // Returns the unit normal of the front side. Throws `std::invalid_argument` for a triangle without area, which has no normal.
    [[nodiscard]] Vec3 getNormal() const;

    // Returns the point of the triangle closest to `point`, on its face, an edge or a corner.
    [[nodiscard]] Vec3 getClosestPoint(Vec3 point) const noexcept;
};

} // namespace haylen::math
