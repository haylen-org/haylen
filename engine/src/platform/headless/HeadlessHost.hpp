#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "haylen/input/Input.hpp"
#include "haylen/math/Insets.hpp"
#include "platform/DevelopmentSession.hpp"
#include "platform/Host.hpp"
#include "platform/NativeViews.hpp"
#include "platform/SystemState.hpp"
#include "platform/headless/HeadlessTextInput.hpp"

namespace haylen::platform {

// Host without a window or GPU. It drives the engine with the Sokol dummy backend so rendering, Lua and the frame loop run in tests.
class HeadlessHost final : public Host {
  public:
    struct PlatformCall {
        std::uint64_t id = 0;
        std::string method;
        std::string paramsJson;
        std::vector<std::vector<std::byte>> buffers;
    };

    // A rumble of a gamepad that the engine asked for.
    struct Rumble {
        std::size_t index = 0;
        float low = 0.0F;
        float high = 0.0F;
        float seconds = 0.0F;
    };

    // A tree of accessible elements the engine handed the platform.
    struct AccessibilityTree {
        std::vector<AccessibilityElement> elements;
        std::uint64_t focused = 0;
    };

    struct Announcement {
        std::string text;
        bool interrupt = false;
    };

    // A native dialog the engine asked for, with the folder where the platform would keep copies of picked files.
    struct DialogCall {
        std::uint64_t id = 0;
        DialogRequest request;
        std::filesystem::path folder;
    };

    explicit HeadlessHost(std::filesystem::path directory, math::Vec2 size = {1920.0F, 1080.0F});

    [[nodiscard]] math::Vec2 getFramebufferSize() const noexcept override {
        return framebufferSize;
    }
    [[nodiscard]] float getDpiScale() const noexcept override {
        return dpiScale;
    }

    // Sets the framebuffer pixels per window point that the host reports, as a dense screen would.
    void setDpiScale(float value) noexcept {
        dpiScale = value;
    }
    [[nodiscard]] bool isFullscreen() const noexcept override {
        return fullscreen;
    }
    void setFullscreen(bool value) override {
        fullscreen = value;
    }
    [[nodiscard]] bool isResizable() const noexcept override {
        return resizable;
    }
    void setResizable(bool value) override {
        resizable = value;
    }
    void setTitle(std::string_view value) override {
        title = value;
    }
    void setCursor(Cursor value) override {
        cursor = value;
    }
    void setCursorVisible(bool value) override {
        cursorVisible = value;
    }
    void setMouseLocked(bool value) override {
        mouseLocked = value;
    }
    void setKeyboardVisible(bool value) override {
        keyboardVisible = value;
    }
    void setClipboard(std::string_view value) override {
        clipboard = value;
    }
    [[nodiscard]] std::string getClipboard() const override {
        return clipboard;
    }
    void requestQuit() override {
        quitRequested = true;
    }

    // The headless screen turns with its framebuffer, like the screen of a phone.
    [[nodiscard]] Orientation getOrientation() const override {
        return framebufferSize.y > framebufferSize.x ? Orientation::Portrait : Orientation::Landscape;
    }
    void lockOrientation(Orientation value) override {
        orientationLock = value;
    }

    // The headless screen has no fold until a test gives it one, as a foldable device reports it.
    [[nodiscard]] std::optional<Fold> getFold() const override {
        return fold;
    }
    void setFold(std::optional<Fold> value) noexcept {
        fold = value;
    }

    // The headless screen has a status bar of the height a test gives it, which covers the top of the screen while it shows, like the status bar of a phone.
    [[nodiscard]] StatusBar getStatusBar() const noexcept override {
        return statusBar;
    }
    void setStatusBar(StatusBar value) override {
        statusBar = value;
    }
    [[nodiscard]] HeadlessTextInput& getTextInput() noexcept override {
        return textInput;
    }
    [[nodiscard]] bool hasPointerDevice() const noexcept override {
        return pointerDevice;
    }
    [[nodiscard]] bool canBeTransparent() const noexcept override {
        return transparencySupported;
    }
    [[nodiscard]] bool isTransparent() const noexcept override {
        return transparent;
    }
    void setTransparent(bool value) override;
    [[nodiscard]] bool isDecorated() const noexcept override {
        return decorated;
    }
    void setDecorated(bool value) override {
        decorated = value;
    }
    [[nodiscard]] bool isAlwaysOnTop() const noexcept override {
        return alwaysOnTop;
    }
    void setAlwaysOnTop(bool value) override {
        alwaysOnTop = value;
    }
    [[nodiscard]] bool isShownInTaskbar() const noexcept override {
        return shownInTaskbar;
    }
    void setShowInTaskbar(bool value) override {
        shownInTaskbar = value;
    }
    [[nodiscard]] bool isFocusable() const noexcept override {
        return focusable;
    }
    void setFocusable(bool value) override {
        focusable = value;
    }
    [[nodiscard]] math::Rect getFrame() const override {
        return frame;
    }
    void setFrame(const math::Rect& value) override {
        frame = value;
    }
    [[nodiscard]] Passthrough getMousePassthrough() const noexcept override {
        return passthrough;
    }
    void setMousePassthrough(Passthrough mode, std::span<const math::Polygon::Outline> regions) override {
        passthrough = mode;
        passthroughRegions.assign(regions.begin(), regions.end());
    }
    void startDrag() override {
        ++dragCount;
    }
    [[nodiscard]] std::vector<Monitor> getMonitors() const override {
        return monitors;
    }

    [[nodiscard]] std::string_view getPlatformName() const noexcept override {
        return "headless";
    }
    [[nodiscard]] graphics::DeviceSetup getGraphicsSetup() override;
    // Without an output backend the mixer has no device and mixes only when a test asks it to.
    [[nodiscard]] audio::Mixer::Setup getAudioSetup() const override {
        return {.device = audioOutput != nullptr, .keepsTime = true, .backend = audioOutput};
    }

    // Gives the mixers of the engines created afterwards a device that opens through the backend, which simulates an audio output that plays, refuses or never answers.
    void setAudioOutput(const audio::OutputBackend* value) noexcept {
        audioOutput = value;
    }
    [[nodiscard]] graphics::FrameTarget getFrameTarget() override;
    [[nodiscard]] std::filesystem::path getUserDataDirectory(std::string_view identifier) override;
    void persistUserData() override {
        ++persistCount;
    }
    [[nodiscard]] math::Insets getSafeAreaInsets() const override {
        return safeAreaInsets;
    }
    [[nodiscard]] float getStatusBarHeight() const override {
        return statusBar == StatusBar::Hidden ? 0.0F : statusBarHeight;
    }
    void pollGamepads(std::span<input::GamepadState> states) override;
    void dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers) override;
    void cancelPlatformCall(std::uint64_t id) override {
        cancelledCalls.push_back(id);
    }
    [[nodiscard]] math::Insets getReservedInsets() const override {
        return nativeViews.getReservedInsets();
    }
    [[nodiscard]] bool isAppCovered() const override {
        return nativeViews.isAppCovered();
    }
    [[nodiscard]] std::vector<std::string> getNativePlugins() const override {
        return nativePlugins;
    }
    void reportError(const core::Json& report) override {
        errorReports.push_back(report);
    }
    [[nodiscard]] SystemInfo getSystemInfo() const override {
        return systemInfo;
    }
    [[nodiscard]] Theme getTheme() const override {
        return systemState.getTheme();
    }
    [[nodiscard]] Battery getBattery() const override {
        return systemState.getBattery();
    }
    [[nodiscard]] AccessibilitySettings getAccessibilitySettings() const override {
        return systemState.getAccessibility();
    }
    [[nodiscard]] bool isAccessibilityTreeWanted() const override {
        return accessibilityWanted;
    }
    void updateAccessibility(std::span<const AccessibilityElement> elements, std::uint64_t focused) override {
        accessibilityTrees.push_back({.elements = {elements.begin(), elements.end()}, .focused = focused});
    }
    void setNowPlaying(const std::optional<MediaSession::NowPlaying>& value) override {
        nowPlaying = value;
    }

    void announce(std::string_view text, bool interrupt) override {
        announcements.push_back({.text = std::string(text), .interrupt = interrupt});
    }

    // The headless platform has no decoder of video files until a test gives it one.
    [[nodiscard]] std::unique_ptr<VideoDecoder> openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) override {
        return videoOpener ? videoOpener(std::move(file), path) : nullptr;
    }
    void openUrl(std::string_view url, std::function<void(bool opened)> callback) override;
    void vibrate(float seconds) override {
        vibrations.push_back(seconds);
    }
    void rumbleGamepad(std::size_t index, float low, float high, float seconds) override {
        rumbles.push_back({.index = index, .low = low, .high = high, .seconds = seconds});
    }
    [[nodiscard]] std::string getNetworkRequirement() const override {
        return networkRequirement;
    }
    void showDialog(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder) override {
        dialogCalls.push_back({.id = id, .request = request, .folder = folder});
    }
    void cancelDialog(std::uint64_t id) override {
        cancelledDialogs.push_back(id);
    }
    void openScreen(const ScreenRequest& request) override {
        screenRequests.push_back(request);
    }
    void cancelScreen(std::uint64_t id) override {
        cancelledScreens.push_back(id);
    }
    void guardCrashes(const std::filesystem::path& report) override {
        crashReport = report;
    }
    void setCrashContext(std::string_view json) override {
        crashContext = json;
    }

    [[nodiscard]] DevelopmentSession* getDevelopmentSession() const noexcept override {
        return developmentSession.get();
    }

    // Plays the apps of this host in development from now on, with a session that scans the package folder when one is given, the way the player does with "--dev", and that connects to a development server when its address is given.
    void enableDevelopment(std::optional<std::filesystem::path> folder = std::nullopt, std::string server = {}) {
        developmentSession = std::make_unique<DevelopmentSession>(std::move(folder), std::move(server));
    }

    void resize(math::Vec2 size) noexcept {
        framebufferSize = size;
    }
    void setSafeAreaInsets(const math::Insets& value) noexcept {
        safeAreaInsets = value;
    }
    void setStatusBarHeight(float value) noexcept {
        statusBarHeight = value;
    }
    void setGamepad(std::size_t index, input::GamepadState state) {
        gamepads.at(index) = std::move(state);
    }
    void setPointerDevice(bool value) noexcept {
        pointerDevice = value;
    }

    // Opens the headless window able to be transparent, and transparent, the way `window.transparent` of `app.json` opens a real one.
    void setTransparencySupported(bool value) noexcept {
        transparencySupported = value;
        transparent = value;
    }
    void setMonitors(std::vector<Monitor> value) {
        monitors = std::move(value);
    }

    // The ids of the plugins whose native part the headless platform reports as loaded.
    void setNativePlugins(std::vector<std::string> value) {
        nativePlugins = std::move(value);
    }

    // What the headless system reports to the apps that start after the change. It starts as a desktop of the operating system the tests run on.
    void setSystemInfo(SystemInfo value) {
        systemInfo = std::move(value);
    }

    // Change the theme, the battery and the accessibility settings from any thread, the way platform services report them. The engine publishes the change at its next frame.
    void setTheme(Theme value) {
        systemState.setTheme(value);
    }
    void setBattery(const Battery& value) {
        systemState.setBattery(value);
    }
    void setAccessibilitySettings(const AccessibilitySettings& value) {
        systemState.setAccessibility(value);
    }

    // Decodes the video files of the apps that open videos from now on, from a worker of the I/O pool, the way the decoder of a platform does.
    using VideoOpener = std::function<std::unique_ptr<VideoDecoder>(std::shared_ptr<io::PackageReader> file, std::string_view path)>;
    void setVideoOpener(VideoOpener value) {
        videoOpener = std::move(value);
    }

    // Whether the headless platform wants the tree of accessible elements, as a platform does while a screen reader runs.
    void setAccessibilityTreeWanted(bool value) noexcept {
        accessibilityWanted = value;
    }

    // What the engine shows in the now-playing controls of the headless platform, or nothing.
    [[nodiscard]] const std::optional<MediaSession::NowPlaying>& getNowPlaying() const noexcept {
        return nowPlaying;
    }

    // The trees of accessible elements and the announcements the engine handed the headless platform, in order.
    [[nodiscard]] const std::vector<AccessibilityTree>& getAccessibilityTrees() const noexcept {
        return accessibilityTrees;
    }
    [[nodiscard]] const std::vector<Announcement>& getAnnouncements() const noexcept {
        return announcements;
    }

    // Whether the headless system finds an app for the urls the engine opens, which it answers at once.
    void setOpensUrls(bool value) noexcept {
        opensUrls = value;
    }

    // What the network errors of the apps that start after the change end with, the way a platform reports what the project of the app lacks for network access.
    void setNetworkRequirement(std::string value) {
        networkRequirement = std::move(value);
    }

    // The native views of the headless screen, which tests reserve edges and cover the app with from any thread, the way native code does. They outlive the engines a test restarts on this host.
    [[nodiscard]] NativeViews& getNativeViews() noexcept {
        return nativeViews;
    }

    [[nodiscard]] const std::string& getTitle() const noexcept {
        return title;
    }
    [[nodiscard]] Cursor getCursor() const noexcept {
        return cursor;
    }
    [[nodiscard]] bool isCursorVisible() const noexcept {
        return cursorVisible;
    }
    [[nodiscard]] bool isMouseLocked() const noexcept {
        return mouseLocked;
    }
    [[nodiscard]] bool isKeyboardVisible() const noexcept {
        return keyboardVisible;
    }
    [[nodiscard]] bool isQuitRequested() const noexcept {
        return quitRequested;
    }
    [[nodiscard]] Orientation getOrientationLock() const noexcept {
        return orientationLock;
    }
    [[nodiscard]] int getPersistCount() const noexcept {
        return persistCount;
    }
    [[nodiscard]] const std::vector<PlatformCall>& getPlatformCalls() const noexcept {
        return platformCalls;
    }
    [[nodiscard]] const std::vector<std::uint64_t>& getCancelledCalls() const noexcept {
        return cancelledCalls;
    }
    [[nodiscard]] const std::vector<math::Polygon::Outline>& getPassthroughRegions() const noexcept {
        return passthroughRegions;
    }
    [[nodiscard]] int getDragCount() const noexcept {
        return dragCount;
    }

    // The JSON reports of the errors that stopped the apps of this host, in order.
    [[nodiscard]] const std::vector<core::Json>& getErrorReports() const noexcept {
        return errorReports;
    }

    [[nodiscard]] const std::vector<std::string>& getOpenedUrls() const noexcept {
        return openedUrls;
    }
    [[nodiscard]] const std::vector<Rumble>& getRumbles() const noexcept {
        return rumbles;
    }
    [[nodiscard]] const std::vector<float>& getVibrations() const noexcept {
        return vibrations;
    }
    [[nodiscard]] const std::vector<DialogCall>& getDialogRequests() const noexcept {
        return dialogCalls;
    }
    [[nodiscard]] const std::vector<std::uint64_t>& getCancelledDialogs() const noexcept {
        return cancelledDialogs;
    }

    // The screens the engine opened on the headless platform and the ids of those it gave up, which tests end through `ScreenRelay::finish` the way native code does.
    [[nodiscard]] const std::vector<ScreenRequest>& getScreenRequests() const noexcept {
        return screenRequests;
    }
    [[nodiscard]] const std::vector<std::uint64_t>& getCancelledScreens() const noexcept {
        return cancelledScreens;
    }

    // The report file and the context that the engine handed the crash guard, which the headless platform never installs.
    [[nodiscard]] const std::filesystem::path& getCrashReport() const noexcept {
        return crashReport;
    }
    [[nodiscard]] const std::string& getCrashContext() const noexcept {
        return crashContext;
    }

  private:
    std::filesystem::path dataDirectory;
    math::Vec2 framebufferSize;
    float dpiScale = 1.0F;
    math::Insets safeAreaInsets{};
    StatusBar statusBar = StatusBar::Hidden;
    float statusBarHeight = 0.0F;
    std::optional<Fold> fold;
    std::array<input::GamepadState, input::Input::kMaxGamepads> gamepads{};
    std::vector<PlatformCall> platformCalls;
    std::vector<std::uint64_t> cancelledCalls;
    NativeViews nativeViews;
    std::vector<std::string> nativePlugins;
    std::vector<core::Json> errorReports;
    SystemInfo systemInfo;
    SystemState systemState;
    std::vector<std::string> openedUrls;
    std::vector<float> vibrations;
    std::vector<Rumble> rumbles;
    std::string networkRequirement;
    std::vector<AccessibilityTree> accessibilityTrees;
    std::vector<Announcement> announcements;
    std::optional<MediaSession::NowPlaying> nowPlaying;
    bool accessibilityWanted = false;
    VideoOpener videoOpener;
    std::vector<DialogCall> dialogCalls;
    std::vector<std::uint64_t> cancelledDialogs;
    std::vector<ScreenRequest> screenRequests;
    std::vector<std::uint64_t> cancelledScreens;
    std::filesystem::path crashReport;
    std::string crashContext;
    std::unique_ptr<DevelopmentSession> developmentSession;
    const audio::OutputBackend* audioOutput = nullptr;
    bool opensUrls = true;

    // The headless desktop is one 1920 by 1080 monitor whose work area leaves a 40 point taskbar at the bottom, with the window at the top left corner of it.
    std::vector<Monitor> monitors{{.name = "headless", .bounds = {0.0F, 0.0F, 1920.0F, 1080.0F}, .workArea = {0.0F, 0.0F, 1920.0F, 1040.0F}, .scale = 1.0F, .primary = true}};
    math::Rect frame;
    std::vector<math::Polygon::Outline> passthroughRegions;
    Passthrough passthrough = Passthrough::Off;
    int dragCount = 0;
    HeadlessTextInput textInput;
    std::string title;
    std::string clipboard;
    Cursor cursor = Cursor::Default;
    Orientation orientationLock = Orientation::Any;
    int persistCount = 0;
    bool fullscreen = false;
    bool resizable = true;
    bool cursorVisible = true;
    bool mouseLocked = false;
    bool keyboardVisible = false;
    bool quitRequested = false;
    bool pointerDevice = true;
    bool transparencySupported = false;
    bool transparent = false;
    bool decorated = true;
    bool alwaysOnTop = false;
    bool shownInTaskbar = true;
    bool focusable = true;
};

} // namespace haylen::platform
