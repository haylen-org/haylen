#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "platform/NativeHost.hpp"
#include "platform/web/WebTextInput.hpp"

namespace haylen::platform::worker {

class Runtime;

// The host of the Worker mode of the web, whose window is the `OffscreenCanvas` of the worker and whose page owns everything of the document: the cursor, the title, fullscreen, the pointer lock, the wake lock, the clipboard and the controllers, which the host asks for through the page side of the runtime.
class Host final : public platform::NativeHost {
  public:
    explicit Host(Runtime& owner) noexcept : NativeHost(WebTextInput::getShared()), runtime(owner) {}

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
    [[nodiscard]] std::unique_ptr<gpu::Device> createGpuDevice() override;
    [[nodiscard]] bool prepareGpuDevice() override;
    [[nodiscard]] audio::Mixer::Setup getAudioSetup() const override;
    void closeWindow() override;
    void appStarted(core::Engine& engine, bool playing) override;
    void appStartFailed(const core::Json& report) override;
    void appStopped(bool playing) override;
    void frameFinished(core::Engine& engine) override;
    void developmentEnabled(DevelopmentSession& session) override;
    [[nodiscard]] bool keepsWindowAfterFailure() const noexcept override;
    [[nodiscard]] bool overlaysEveryPackage() const noexcept override;
    void endSplashScreen(float fadeOutSeconds) override;
    [[nodiscard]] bool canHaltFrames() const noexcept override;
    [[nodiscard]] bool pumpHaltedEvents() override;
    void pollGamepads(std::span<input::GamepadState> gamepads) override;
    void rumbleGamepad(std::size_t index, float low, float high, float seconds) override;
    void setGamepadMotion(input::GamepadMotion::Sensors sensors) override;
    void openOutput(std::uint32_t number, std::string_view display, const Output::Options& options) override;
    void closeOutput(std::uint32_t number) override;
    [[nodiscard]] OutputReport getOutputReport(std::uint32_t number) const override;

  protected:
    [[nodiscard]] bool allowsQuit() const noexcept override {
        return false;
    }
    void requestWindowClose() override;
    void setWindowFocusable(bool value) override;
    void setScreenSaverSuspended(bool value, std::function<void(Permission::Answer answer)> callback) override;
    void applyFrameRate(int framesPerSecond) override;
    void applyMaxPixelDensity(float cap) override;

  private:
    [[nodiscard]] static std::string_view toCssCursor(Cursor cursor) noexcept;

    Runtime& runtime;
    Cursor cursor = Cursor::Default;
    bool cursorVisible = true;
};

} // namespace haylen::platform::worker
