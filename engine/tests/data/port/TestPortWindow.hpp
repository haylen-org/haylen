#pragma once

#include "haylen/gpu/Surface.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::platform {

// The window of the test port, which has no handle in a system and only the size of its framebuffer, on a screen of one pixel in each point, so only a pixel density cap below 1 shrinks its drawable.
class TestPortWindow final {
  public:
    static constexpr math::Vec2 kSize{1280.0F, 720.0F};

    void setMaxPixelDensity(float value) noexcept {
        maxPixelDensity = value;
    }

    [[nodiscard]] gpu::Surface::Size getSurfaceSize() const noexcept;

  private:
    float maxPixelDensity = 0.0F;
};

} // namespace haylen::platform
