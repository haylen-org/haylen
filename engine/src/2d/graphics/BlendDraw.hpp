#pragma once

#include "haylen/2d/graphics/ImageBlend.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// An image blend as the frame draws it: the corner where its images start and the edges along their rows and columns, in the coordinates of its canvas, which follow a turned or stretched view, and the opacity of the result.
struct BlendDraw {
    ImageBlend blend;
    math::Vec2 origin{};
    math::Vec2 rows{};
    math::Vec2 columns{};
    float opacity = 1.0F;
};

} // namespace haylen::graphics2d
