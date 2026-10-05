#pragma once

#include "2d/graphics/LightDraw.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::graphics2d {

// One light as the light program reads it from the light buffer, where the lights of a canvas that share a blend mode and a shape draw as instances of one call.
struct LightInstance {
    float area[4];
    float color[4];
    float shape[4];
    float cone[4];
    float origin[4];
    float range[4];
    float shadowColor[4];
    float shadowMap[4];
    float shadowAxis[4];

    // Packs a light of a canvas whose world bounds a directional light covers.
    [[nodiscard]] static LightInstance make(const LightDraw& draw, const math::Rect& bounds) noexcept;
};

} // namespace haylen::graphics2d
