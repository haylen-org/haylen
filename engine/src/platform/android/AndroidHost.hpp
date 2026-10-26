#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "platform/NativeHost.hpp"

namespace haylen::platform {

class AndroidRuntime;

// The host of Android apps, whose window is the surface of the `GameActivity` that `AndroidRuntime` runs. The window always fills the screen, so it is always fullscreen and has no title, no cursor and no style.
class AndroidHost final : public NativeHost {
  public:
    explicit AndroidHost(AndroidRuntime& activityRuntime) noexcept : runtime(activityRuntime) {}

    [[nodiscard]] math::Vec2 getFramebufferSize() const noexcept override;
    [[nodiscard]] float getDpiScale() const noexcept override;
    [[nodiscard]] bool isFullscreen() const noexcept override {
        return true;
    }
    void setFullscreen(bool) override {}
    void setTitle(std::string_view) override {}
    void setCursor(Cursor) override {}
    void setCursorVisible(bool) override {}
    void setMouseLocked(bool) override {}
    void setClipboard(std::string_view value) override;
    [[nodiscard]] std::string getClipboard() const override;
    [[nodiscard]] bool canBeTransparent() const noexcept override {
        return false;
    }
    [[nodiscard]] TextInput& getTextInput() noexcept override;
    [[nodiscard]] std::unique_ptr<gpu::Device> createGpuDevice() override;
    void closeWindow() override;
    void pollGamepads(std::span<input::GamepadState> gamepads) override;
    void rumbleGamepad(std::size_t index, float low, float high, float seconds) override;

  protected:
    void requestWindowClose() override;
    void setWindowFocusable(bool) override {}

  private:
    AndroidRuntime& runtime;
};

} // namespace haylen::platform
