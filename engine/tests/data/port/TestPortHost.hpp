#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "haylen/platform/TextInput.hpp"
#include "haylen/port/Host.hpp"

namespace haylen::platform {

class TestPortRuntime;

// The window of the test port, which has a fixed size and no cursor, clipboard of the system or controllers, types through key and character events and plays without sound, and whose GPU device the runtime makes.
class TestPortHost final : public port::Host {
  public:
    explicit TestPortHost(TestPortRuntime& owner) noexcept : runtime(owner) {}

    [[nodiscard]] math::Vec2 getFramebufferSize() const noexcept override;
    [[nodiscard]] float getDpiScale() const noexcept override;
    [[nodiscard]] bool isFullscreen() const noexcept override;
    void setFullscreen(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void setTitle(std::string_view value) override;
    void setCursor(Window::Cursor value) override;
    void setCursorVisible(bool value) override;
    void setMouseLocked(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void setClipboard(std::string_view value, std::function<void(Permission::Answer answer)> callback) override;
    [[nodiscard]] std::string getClipboard() const override;
    void readClipboard(std::function<void(Permission::Answer answer, std::string text)> callback) override;
    [[nodiscard]] bool canBeTransparent() const noexcept override;
    [[nodiscard]] TextInput& getTextInput() noexcept override;
    [[nodiscard]] bool prepareGpuDevice() override;
    [[nodiscard]] std::unique_ptr<gpu::Device> createGpuDevice() override;
    [[nodiscard]] audio::Mixer::Setup getAudioSetup() const override;
    void requestClose() override;
    void close() override;
    [[nodiscard]] bool canQuit() const noexcept override;
    void endSplashScreen(float fadeOutSeconds) override;
    [[nodiscard]] bool pumpHaltedEvents() override;
    void pollGamepads(std::span<input::GamepadState> gamepads) override;
    void rumbleGamepad(std::size_t index, float low, float high, float seconds) override;
    void setGamepadMotion(input::GamepadMotion::Sensors sensors) override;
    void setFocusable(bool value) override;
    void setScreenSaverSuspended(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void applyFrameRate(int framesPerSecond) override;
    void applyMaxPixelDensity(float cap) override;

  private:
    TestPortRuntime& runtime;
    TextInput textInput;
    std::string clipboard;
};

} // namespace haylen::platform
