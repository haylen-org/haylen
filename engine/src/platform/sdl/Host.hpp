#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include "platform/NativeHost.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if !defined(__EMSCRIPTEN__) && (!defined(__APPLE__) || TARGET_OS_OSX)
#include "platform/sdl/Sensors.hpp"
#endif

namespace haylen::platform::sdl {

class Runtime;

// The host of the platforms that SDL runs, whose window SDL opens and whose frames the GPU device of the graphics API that the runtime chose draws.
class Host final : public platform::NativeHost {
  public:
    explicit Host(Runtime& owner) noexcept : runtime(owner) {}
    ~Host() override;

    [[nodiscard]] math::Vec2 getFramebufferSize() const noexcept override;
    [[nodiscard]] float getDpiScale() const noexcept override;
    [[nodiscard]] bool isFullscreen() const noexcept override;
    void setFullscreen(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void enterFullscreen(const Fullscreen& value, std::function<void(Permission::Answer answer)> callback) override;
    void setTitle(std::string_view value) override;
    void setCursor(Cursor value) override;
    void setCursorVisible(bool value) override;
    void setMouseLocked(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void setClipboard(std::string_view value, std::function<void(Permission::Answer answer)> callback) override;
    [[nodiscard]] std::string getClipboard() const override;
    void readClipboard(std::function<void(Permission::Answer answer, std::string text)> callback) override;
    [[nodiscard]] bool canBeTransparent() const noexcept override;
    [[nodiscard]] platform::TextInput& getTextInput() noexcept override;
    [[nodiscard]] std::unique_ptr<gpu::Device> createGpuDevice() override;
    [[nodiscard]] bool prepareGpuDevice() override;
    void closeWindow() override;
    [[nodiscard]] bool canHaltFrames() const noexcept override;
    [[nodiscard]] bool pumpHaltedEvents() override;
    void pollGamepads(std::span<input::GamepadState> gamepads) override;
    void rumbleGamepad(std::size_t index, float low, float high, float seconds) override;
    [[nodiscard]] input::MotionSensors::Support getMotionSupport() override;
    void setMotion(input::MotionSensors::Sensors sensors, float rate) override;
    void readMotion(input::MotionState& state) override;
    void setGamepadMotion(input::MotionSensors::Sensors sensors) override;
    void openOutput(std::uint32_t number, std::string_view display, const Output::Options& options) override;
    void closeOutput(std::uint32_t number) override;
    [[nodiscard]] OutputReport getOutputReport(std::uint32_t number) const override;

  protected:
    void requestWindowClose() override;
    void setWindowFocusable(bool value) override;
    void setScreenSaverSuspended(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void applyFrameRate(int framesPerSecond) override;

  private:
    static constexpr std::size_t kCursorCount = static_cast<std::size_t>(Cursor::NotAllowed) + 1;

    [[nodiscard]] static SDL_SystemCursor toSystemCursor(Cursor cursor) noexcept;
    [[nodiscard]] static std::optional<SDL_DisplayMode> findMode(SDL_DisplayID display, const Display::Mode& mode);
    void enterDesktopFullscreen(const FullscreenTarget& target, std::function<void(Permission::Answer answer)> callback);

    Runtime& runtime;

    // The cursors of the system, which SDL makes the first time the app shows each one.
    std::array<SDL_Cursor*, kCursorCount> cursors{};
#if !defined(__EMSCRIPTEN__) && (!defined(__APPLE__) || TARGET_OS_OSX)
    Sensors sensors;
#endif
};

} // namespace haylen::platform::sdl
