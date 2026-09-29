#pragma once

#include "sokol_gfx.h"

namespace haylen::graphics {

// The swapchain a frame presents to, which the platform host provides every frame. Only a transparent window composes the alpha of the frame with the desktop, and every other frame keeps alpha 1 whatever it draws.
struct FrameTarget {
    sg_swapchain swapchain{};
    bool transparent = false;
};

} // namespace haylen::graphics
