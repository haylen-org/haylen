#pragma once

#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// A render target that the frame shows in the swapchain of a surface beyond the window, such as an output on another display, once every canvas drew, as large as it fits in the surface with bars of the clear color around it.
struct Presentation {
    graphics::RenderTarget target;
    int surface = 0;
    math::Vec2 size;
    math::Color clear;
};

} // namespace haylen::graphics2d
