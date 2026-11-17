#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "haylen/port/Host.hpp"
#include "platform/NativeHost.hpp"

namespace haylen::port {

// The native host of a port, which keeps what the engine holds of every host and hands the window of the port what the window does. Inside it, `Host` names the host of the engine that it derives from, so the window of the port is always `port::Host`.
class PortHost final : public platform::NativeHost {
  public:
    explicit PortHost(std::unique_ptr<port::Host> window) noexcept : NativeHost(window->getTextInput()), host(std::move(window)) {}

    [[nodiscard]] math::Vec2 getFramebufferSize() const noexcept override;
    [[nodiscard]] float getDpiScale() const noexcept override;
    [[nodiscard]] bool isFullscreen() const noexcept override;
    void setFullscreen(bool value, std::function<void(platform::Permission::Answer answer)> callback) override;
    void setTitle(std::string_view value) override;
    void setCursor(Cursor value) override;
    void setCursorVisible(bool value) override;
    void setMouseLocked(bool value, std::function<void(platform::Permission::Answer answer)> callback) override;
    void setClipboard(std::string_view value, std::function<void(platform::Permission::Answer answer)> callback) override;
    [[nodiscard]] std::string getClipboard() const override;
    void readClipboard(std::function<void(platform::Permission::Answer answer, std::string text)> callback) override;
    [[nodiscard]] bool canBeTransparent() const noexcept override;
    [[nodiscard]] std::unique_ptr<gpu::Device> createGpuDevice() override;
    [[nodiscard]] bool prepareGpuDevice() override;
    [[nodiscard]] audio::Mixer::Setup getAudioSetup() const override;
    void closeWindow() override;
    void endSplashScreen(float fadeOutSeconds) override;
    [[nodiscard]] bool pumpHaltedEvents() override;
    void pollGamepads(std::span<input::GamepadState> gamepads) override;
    void rumbleGamepad(std::size_t index, float low, float high, float seconds) override;
    void setGamepadMotion(input::GamepadMotion::Sensors sensors) override;

  protected:
    [[nodiscard]] bool allowsQuit() const noexcept override;
    void requestWindowClose() override;
    void setWindowFocusable(bool value) override;
    void setScreenSaverSuspended(bool value, std::function<void(platform::Permission::Answer answer)> callback) override;
    void applyFrameRate(int framesPerSecond) override;
    void applyMaxPixelDensity(float cap) override;

  private:
    std::unique_ptr<port::Host> host;
};

} // namespace haylen::port
