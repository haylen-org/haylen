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
#include "platform/sdl/FullscreenRequest.hpp"

namespace haylen::platform::sdl {

class Runtime;

// The host of the platforms that SDL runs, whose window SDL opens and whose frames the GPU device of the graphics API that the runtime chose draws.
class Host final : public platform::NativeHost {
  public:
    explicit Host(Runtime& owner) noexcept : NativeHost(getPlatformTextInput(owner)), runtime(owner) {}
    ~Host() override;

    [[nodiscard]] math::Vec2 getFramebufferSize() const noexcept override;
    [[nodiscard]] float getDpiScale() const noexcept override;
    [[nodiscard]] bool isFullscreen() const noexcept override;
    void setTitle(std::string_view value) override;
    void setCursor(Cursor value) override;
    void setCursorVisible(bool value) override;
    void setClipboard(std::string_view value, std::function<void(SystemAnswer answer)> callback) override;
    [[nodiscard]] std::string getClipboard() const override;
    void readClipboard(std::function<void(SystemAnswer answer, std::string text)> callback) override;
    [[nodiscard]] bool canBeTransparent() const noexcept override;
    [[nodiscard]] std::unique_ptr<gpu::Device> createGpuDevice() override;
    [[nodiscard]] bool prepareGpuDevice() override;
    [[nodiscard]] audio::Mixer::Setup getAudioSetup() const override;
    void closeWindow() override;
    void appStarted(core::Engine& engine, bool playing) override;
    void appStartFailed(const core::Json& report) override;
    void appStopping() override;
    void appStopped(bool playing) override;
    void frameFinished(core::Engine& engine) override;
    void developmentEnabled(DevelopmentSession& session) override;
    [[nodiscard]] bool keepsWindowAfterFailure() const noexcept override;
    [[nodiscard]] bool overlaysEveryPackage() const noexcept override;
    [[nodiscard]] std::vector<Display> getDisplays() const override;
    [[nodiscard]] std::string getDisplayId() const override;
    void endSplashScreen(float fadeOutSeconds) override;
    [[nodiscard]] bool canHaltFrames() const noexcept override;
    [[nodiscard]] bool pumpHaltedEvents() override;
    void pollGamepads(std::span<input::GamepadState> gamepads) override;
    void rumbleGamepad(std::size_t index, float low, float high, float seconds) override;
    void setGamepadMotion(input::GamepadMotion::Sensors sensors) override;
    void openOutput(std::uint32_t number, std::string_view display, const Output::Options& options) override;
    void closeOutput(std::uint32_t number) override;
    [[nodiscard]] OutputReport getOutputReport(std::uint32_t number) const override;
    void setExternalDisplays(bool value) override;

    // Follows the window as SDL reports that it entered or left fullscreen, whoever asked for it, and answers the request of the app that waited for that mode.
    void followFullscreen(bool entered);

  protected:
    void applyFullscreen(bool value, std::function<void(SystemAnswer answer)> callback) override;
    void applyFullscreenTarget(const Fullscreen& value, std::function<void(SystemAnswer answer)> callback) override;
    void applyMouseLocked(bool value, std::function<void(SystemAnswer answer)> callback) override;
    [[nodiscard]] bool allowsQuit() const noexcept override;
    void requestWindowClose() override;
    void setWindowFocusable(bool value) override;
    void setScreenSaverSuspended(bool value, std::function<void(SystemAnswer answer)> callback) override;
    void applyFrameRate(int framesPerSecond) override;
    void applyMaxPixelDensity(float cap) override;

  private:
    // Apple platforms and the web edit text in a native field of their own, while Windows and Linux type through the text events of SDL.
    [[nodiscard]] static platform::TextInput& getPlatformTextInput(Runtime& owner) noexcept;

    static constexpr std::size_t kCursorCount = static_cast<std::size_t>(Cursor::NotAllowed) + 1;

    [[nodiscard]] static SDL_SystemCursor toSystemCursor(Cursor cursor) noexcept;
    [[nodiscard]] static std::optional<SDL_DisplayMode> findMode(SDL_DisplayID display, const Display::Mode& mode);
    void enterDesktopFullscreen(const FullscreenTarget& target, std::function<void(SystemAnswer answer)> callback);
    void coverDisplay(const FullscreenRequest::Destination& destination);

    Runtime& runtime;
    FullscreenRequest fullscreenRequest;

    // The cursors of the system, which SDL makes the first time the app shows each one.
    std::array<SDL_Cursor*, kCursorCount> cursors{};
};

} // namespace haylen::platform::sdl
