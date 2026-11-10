#pragma once

#include "haylen/gpu/Surface.hpp"
#include "haylen/math/Vec2.hpp"
#include "platform/NativeWindow.hpp"

namespace haylen::platform {

// The window of the test port, which has no handle in a system and only the size of its framebuffer, on a screen of one pixel in each point, so only a pixel density cap below 1 shrinks its drawable.
class TestPortWindow final : public NativeWindow {
  public:
    static constexpr math::Vec2 kSize{1280.0F, 720.0F};

    void setOpen(bool value) noexcept {
        open = value;
    }

    void setMaxPixelDensity(float value) noexcept {
        maxPixelDensity = value;
    }

    [[nodiscard]] gpu::Surface::Size getSurfaceSize() const noexcept;

    [[nodiscard]] bool queryOpen() const noexcept override;
    [[nodiscard]] void* queryHandle() const noexcept override;
    [[nodiscard]] void* queryDisplay() const noexcept override;
    [[nodiscard]] void* queryActivity() const noexcept override;
    [[nodiscard]] math::Vec2 queryFramebufferSize() const noexcept override;
    [[nodiscard]] float queryScale() const noexcept override;

  private:
    bool open = false;
    float maxPixelDensity = 0.0F;
};

} // namespace haylen::platform
