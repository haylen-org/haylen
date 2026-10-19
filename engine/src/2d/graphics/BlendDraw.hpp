#pragma once

#include "haylen/2d/graphics/ImageBlend.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// An image blend as the frame draws it: the corner where its images start and the edges along their rows and columns, in the coordinates of its canvas, which follow a turned or stretched view, the opacity of the result, and whether its first image holds premultiplied colors that its blend takes straight, as the image of a group that adds, multiplies or screens.
struct BlendDraw {
    ImageBlend blend;
    math::Vec2 origin{};
    math::Vec2 rows{};
    math::Vec2 columns{};
    float opacity = 1.0F;
    bool straighten = false;
};

} // namespace haylen::graphics2d
