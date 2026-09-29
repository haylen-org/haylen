#pragma once

#include <cstddef>

#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::graphics2d {

// A capture of the frame. The world and screen canvases that began while it was open render into its target, and it renders once every canvas before its end has its own offscreen passes.
struct Capture {
    graphics::RenderTarget target;
    math::Color clear = math::Color::transparent();
    std::size_t canvasEnd = 0;
};

} // namespace haylen::graphics2d
