#pragma once

#include "sokol_gfx.h"

namespace haylen::graphics {

// The swapchain a frame presents to, which the platform host provides every frame.
struct FrameTarget {
    sg_swapchain swapchain{};
};

} // namespace haylen::graphics
