#pragma once

#include "haylen/math/Vec2.hpp"
#include "platform/NativeWindow.hpp"

namespace haylen::platform {

// The window of the test port, which has no handle in a system and only the size of its framebuffer.
class TestPortWindow final : public NativeWindow {
  public:
    static constexpr math::Vec2 kSize{1280.0F, 720.0F};

    void setOpen(bool value) noexcept {
        open = value;
    }

    [[nodiscard]] bool queryOpen() const noexcept override;
    [[nodiscard]] void* queryHandle() const noexcept override;
    [[nodiscard]] void* queryDisplay() const noexcept override;
    [[nodiscard]] void* queryActivity() const noexcept override;
    [[nodiscard]] math::Vec2 queryFramebufferSize() const noexcept override;
    [[nodiscard]] float queryScale() const noexcept override;

  private:
    bool open = false;
};

} // namespace haylen::platform
