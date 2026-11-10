#pragma once

#include "haylen/gpu/Surface.hpp"

namespace haylen::platform {

// The drawable of a window whose app caps its pixel density, which keeps at most the cap of framebuffer pixels in each point of the window. The window keeps its size in points and pixels, and the system or the GPU device stretches the smaller drawable over it.
class PixelDensityCap final {
  public:
    // The drawable of a window of the size in pixels with the pixels per point of its display: the window itself for a cap of 0 or a display that is not denser than the cap, and otherwise at least one pixel on each side.
    [[nodiscard]] static gpu::Surface::Size apply(int windowWidth, int windowHeight, float density, float cap) noexcept;

    // The share of the pixels of the window across that the drawable keeps, which scales the pixels per point and per unit of the window down to those of the drawable.
    [[nodiscard]] static float getShare(const gpu::Surface::Size& size) noexcept;
};

} // namespace haylen::platform
