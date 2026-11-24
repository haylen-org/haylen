#pragma once

#include "haylen/math/Vec3.hpp"

namespace haylen::math {

struct Segment3D {
    Vec3 start;
    Vec3 end;

    [[nodiscard]] constexpr bool operator==(const Segment3D&) const noexcept = default;

    [[nodiscard]] float getLength() const noexcept {
        return Vec3::distance(start, end);
    }

    // Returns the point of the segment closest to `point`.
    [[nodiscard]] constexpr Vec3 getClosestPoint(Vec3 point) const noexcept {
        const Vec3 along = end - start;
        const float squared = along.getLengthSquared();
        if (squared == 0.0F) {
            return start;
        }

        const float t = Vec3::dot(point - start, along) / squared;
        return start + along * (t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t));
    }
};

} // namespace haylen::math
