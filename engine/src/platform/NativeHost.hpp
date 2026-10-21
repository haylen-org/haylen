#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "haylen/math/Insets.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/DevelopmentSession.hpp"
#include "platform/Host.hpp"
#include "platform/KeyboardTranslator.hpp"
#include "platform/NativeViews.hpp"
#include "platform/SystemState.hpp"
#include "platform/WindowStyle.hpp"

namespace haylen::platform {

// The host of every backend on a real device, which reaches the system through the services of the platform folder compiled into the runtime. A backend derives its host from it and adds what its window does: its size, scale, fullscreen, title, cursor, clipboard, closing, focus, transparency and GPU device.
class NativeHost : public Host {
  public:
    [[nodiscard]] bool isResizable() const noexcept override;
    void setResizable(bool value) override;
    void setKeyboardVisible(bool value) override;
    void requestQuit() override;
    [[nodiscard]] bool canQuit() const noexcept override;
    [[nodiscard]] Orientation getOrientation() const override;
    void lockOrientation(Orientation value) override;
    [[nodiscard]] std::optional<Fold> getFold() const override;
    [[nodiscard]] StatusBar getStatusBar() const noexcept override;
    void setStatusBar(StatusBar value) override;
    [[nodiscard]] TextInput& getTextInput() noexcept override;
    [[nodiscard]] bool hasPointerDevice() const noexcept override;
    [[nodiscard]] bool isTransparent() const noexcept override;
    void setTransparent(bool value) override;
    [[nodiscard]] bool isDecorated() const noexcept override;
    void setDecorated(bool value) override;
    [[nodiscard]] bool isAlwaysOnTop() const noexcept override;
    void setAlwaysOnTop(bool value) override;
    [[nodiscard]] bool isShownInTaskbar() const noexcept override;
    void setShowInTaskbar(bool value) override;
    [[nodiscard]] bool isFocusable() const noexcept override;
    void setFocusable(bool value) override;
    [[nodiscard]] math::Rect getFrame() const override;
    void setFrame(const math::Rect& value) override;
    [[nodiscard]] Passthrough getMousePassthrough() const noexcept override;
    void setMousePassthrough(Passthrough mode, std::span<const math::Polygon::Outline> regions) override;
    void startDrag() override;
    [[nodiscard]] std::vector<Monitor> getMonitors() const override;

    [[nodiscard]] std::string_view getPlatformName() const noexcept override;
    [[nodiscard]] audio::Mixer::Setup getAudioSetup() const override;
    [[nodiscard]] bool isFrameTransparent() const noexcept override;
    [[nodiscard]] std::filesystem::path getUserDataDirectory(std::string_view identifier) override;
    void persistUserData() override;
    [[nodiscard]] math::Insets getSafeAreaInsets() const override;
    [[nodiscard]] float getStatusBarHeight() const override;
    void pollGamepads(std::span<input::GamepadState> gamepads) override;
    void dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers) override;
    void cancelPlatformCall(std::uint64_t id) override;
    [[nodiscard]] math::Insets getReservedInsets() const override;
    [[nodiscard]] bool isAppCovered() const override;
    [[nodiscard]] std::vector<std::string> getNativePlugins() const override;
    void reportError(const core::Json& report) override;
    [[nodiscard]] SystemInfo getSystemInfo() const override;
    [[nodiscard]] Theme getTheme() const override;
    [[nodiscard]] Battery getBattery() const override;
    [[nodiscard]] AccessibilitySettings getAccessibilitySettings() const override;
    [[nodiscard]] bool isAccessibilityTreeWanted() const override;
    void updateAccessibility(std::span<const AccessibilityElement> elements, std::uint64_t focused) override;
    void setNowPlaying(const std::optional<MediaSession::NowPlaying>& value) override;
    void announce(std::string_view text, bool interrupt) override;
    [[nodiscard]] std::unique_ptr<VideoDecoder> openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) override;
    void openUrl(std::string_view url, std::function<void(bool opened)> callback) override;
    void vibrate(float seconds) override;
    void rumbleGamepad(std::size_t index, float low, float high, float seconds) override;
    void guardCrashes(const std::filesystem::path& report) override;
    void setCrashContext(std::string_view json) override;
    [[nodiscard]] std::string getNetworkRequirement() const override;
    void showDialog(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder) override;
    void cancelDialog(std::uint64_t id) override;
    void openScreen(const ScreenRequest& request) override;
    void cancelScreen(std::uint64_t id) override;
    [[nodiscard]] DevelopmentSession* getDevelopmentSession() const noexcept override {
        return developmentSession.get();
    }
    [[nodiscard]] bool isLaunched() const noexcept override {
        return launched;
    }
    void setLaunched(bool value) noexcept {
        launched = value;
    }

    // Plays the apps of this runtime in development, with a session that scans the package folder when the app plays from one and connects to the development server whose address the launch gave.
    void enableDevelopment(std::optional<std::filesystem::path> folder, std::string server) {
        developmentSession = std::make_unique<DevelopmentSession>(std::move(folder), std::move(server));
    }

    // The native views of plugins over the app, which the platform services update from any thread. They belong to the process, so a reservation or a cover outlives the apps that restart under it.
    [[nodiscard]] static NativeViews& getNativeViews() noexcept;

    // The theme, the battery and the accessibility settings that the platform services report from any thread, at initialize and whenever the system changes them. They belong to the process, so every app that starts hears the last report.
    [[nodiscard]] static SystemState& getSystemState() noexcept;

    // Records the options the window opens with, before it exists, so the options the engine applies when the app starts find the window as it already is.
    void prepare(const WindowStyle& openingStyle, bool openingFocusable) noexcept {
        style = openingStyle;
        focusable = openingFocusable;
    }

    // Turns the edits and actions of the plain keyboard that `setKeyboardVisible` opens into key and character events.
    [[nodiscard]] std::vector<Event> translateKeyboard(const Event& event) {
        return keyboard.translate(event);
    }

    // Closes the window at once, without asking the app, which ends the process of desktop apps.
    virtual void closeWindow() = 0;

  protected:
    // Asks the window to close as its close button does, which the app hears before it closes.
    virtual void requestWindowClose() = 0;
    virtual void setWindowFocusable(bool value) = 0;

    // Fullscreen windows keep the style of the platform, and leaving fullscreen brings this one back.
    void applyStyle();

  private:
    KeyboardTranslator keyboard;
    std::unique_ptr<DevelopmentSession> developmentSession;
    bool launched = false;
    std::uint64_t keyboardRevision = 0;
    WindowStyle style;
    Passthrough passthrough = Passthrough::Off;
    StatusBar statusBar = StatusBar::Hidden;
    bool focusable = true;
};

} // namespace haylen::platform
