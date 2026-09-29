#pragma once

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {

struct Circle {
    Vec2 center{};
    float radius = 0.0F;

    [[nodiscard]] constexpr bool contains(Vec2 point) const noexcept {
        return Vec2::distanceSquared(center, point) <= radius * radius;
    }
    [[nodiscard]] constexpr Rect getBounds() const noexcept {
        return {center.x - radius, center.y - radius, radius * 2.0F, radius * 2.0F};
    }
};

} // namespace haylen::math
