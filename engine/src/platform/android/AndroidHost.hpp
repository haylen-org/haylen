#pragma once

#include <cstddef>
#include <functional>
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
    void setFullscreen(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void setTitle(std::string_view) override {}
    void setCursor(Cursor) override {}
    void setCursorVisible(bool) override {}
    void setMouseLocked(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void setClipboard(std::string_view value, std::function<void(Permission::Answer answer)> callback) override;
    [[nodiscard]] std::string getClipboard() const override;
    void readClipboard(std::function<void(Permission::Answer answer, std::string text)> callback) override;
    [[nodiscard]] bool canBeTransparent() const noexcept override {
        return false;
    }
    [[nodiscard]] TextInput& getTextInput() noexcept override;
    [[nodiscard]] std::unique_ptr<gpu::Device> createGpuDevice() override;
    [[nodiscard]] bool prepareGpuDevice() override;
    void closeWindow() override;
    void endSplashScreen(float fadeOutSeconds) override;
    [[nodiscard]] bool pumpHaltedEvents() override;
    void pollGamepads(std::span<input::GamepadState> gamepads) override;
    void rumbleGamepad(std::size_t index, float low, float high, float seconds) override;
    void setGamepadMotion(input::GamepadMotion::Sensors sensors) override;

  protected:
    void requestWindowClose() override;
    void setWindowFocusable(bool) override {}
    void setScreenSaverSuspended(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void applyFrameRate(int framesPerSecond) override;
    void openOutput(std::uint32_t number, std::string_view display, const Output::Options& options) override;
    void closeOutput(std::uint32_t number) override;
    [[nodiscard]] OutputReport getOutputReport(std::uint32_t number) const override;

  private:
    AndroidRuntime& runtime;
};

} // namespace haylen::platform
