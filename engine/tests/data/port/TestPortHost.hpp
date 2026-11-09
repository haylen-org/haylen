#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "platform/NativeHost.hpp"

namespace haylen::platform {

class TestPortRuntime;

// The host of the test port, whose window has a fixed size and no cursor, clipboard, controllers or motion sensors, and whose GPU device the runtime makes.
class TestPortHost final : public NativeHost {
  public:
    explicit TestPortHost(TestPortRuntime& owner) noexcept : runtime(owner) {}

    [[nodiscard]] math::Vec2 getFramebufferSize() const noexcept override;
    [[nodiscard]] float getDpiScale() const noexcept override;
    [[nodiscard]] bool isFullscreen() const noexcept override;
    void setFullscreen(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void setTitle(std::string_view value) override;
    void setCursor(Cursor value) override;
    void setCursorVisible(bool value) override;
    void setMouseLocked(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void setClipboard(std::string_view value, std::function<void(Permission::Answer answer)> callback) override;
    [[nodiscard]] std::string getClipboard() const override;
    void readClipboard(std::function<void(Permission::Answer answer, std::string text)> callback) override;
    [[nodiscard]] bool canBeTransparent() const noexcept override;
    [[nodiscard]] TextInput& getTextInput() noexcept override;
    [[nodiscard]] std::unique_ptr<gpu::Device> createGpuDevice() override;
    [[nodiscard]] bool prepareGpuDevice() override;
    [[nodiscard]] audio::Mixer::Setup getAudioSetup() const override;
    void closeWindow() override;
    void endSplashScreen(float fadeOutSeconds) override;
    [[nodiscard]] bool pumpHaltedEvents() override;
    void pollGamepads(std::span<input::GamepadState> gamepads) override;
    void rumbleGamepad(std::size_t index, float low, float high, float seconds) override;
    [[nodiscard]] input::MotionSensors::Support getMotionSupport() override;
    void setMotion(input::MotionSensors::Sensors sensors, float rate) override;
    void readMotion(input::MotionState& state) override;
    void setGamepadMotion(input::MotionSensors::Sensors sensors) override;

  protected:
    void requestWindowClose() override;
    void setWindowFocusable(bool value) override;
    void setScreenSaverSuspended(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void applyFrameRate(int framesPerSecond) override;

  private:
    TestPortRuntime& runtime;
    std::string clipboard;
};

} // namespace haylen::platform
