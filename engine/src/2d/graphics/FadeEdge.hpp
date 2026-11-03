#pragma once

#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// An edge of a fade, which covers a point of the canvas by the dot product of the slope with the point plus the offset, clamped to 0 and 1. The coverage grows from the strength left at the edge to 1 at the length of the fade inside it.
struct FadeEdge {
    math::Vec2 slope{};
    float offset = 1.0F;

    // Returns how much of a draw at the point the edge keeps, before the clamp.
    [[nodiscard]] float getCoverage(math::Vec2 point) const noexcept {
        return slope.x * point.x + slope.y * point.y + offset;
    }

    friend bool operator==(const FadeEdge&, const FadeEdge&) = default;
};

} // namespace haylen::graphics2d
