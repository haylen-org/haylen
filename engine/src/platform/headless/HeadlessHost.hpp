#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/math/Insets.hpp"
#include "platform/DevelopmentSession.hpp"
#include "platform/Host.hpp"
#include "platform/NativeViews.hpp"
#include "platform/PixelDensityCap.hpp"
#include "platform/SystemState.hpp"
#include "platform/headless/HeadlessTextInput.hpp"

namespace haylen::platform {

// Host without a window or GPU. It drives the engine with the null GPU device so rendering, Lua and the frame loop run in tests.
class HeadlessHost final : public Host {
  public:
    struct PlatformCall {
        std::uint64_t id = 0;
        std::string method;
        std::string paramsJson;
        std::vector<std::vector<std::byte>> buffers;
        std::vector<varn::dataview::DataView> views;
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

    explicit HeadlessHost(std::filesystem::path directory, math::Vec2 size = {1920.0F, 1080.0F});

    [[nodiscard]] math::Vec2 getFramebufferSize() const noexcept override {
        const gpu::Surface::Size size = getSurfaceSize();
        return {static_cast<float>(size.width), static_cast<float>(size.height)};
    }
    [[nodiscard]] float getDpiScale() const noexcept override {
        return dpiScale * PixelDensityCap::getShare(getSurfaceSize());
    }

    // Sets the pixels per window point of the headless screen, as a dense screen would, which the framebuffer keeps unless the pixel density cap lowers it.
    void setDpiScale(float value) noexcept {
        dpiScale = value;
    }

    // The size of the drawable and of the window, which the null device of the host takes when it is made and whenever the pixel density cap changes.
    [[nodiscard]] gpu::Surface::Size getSurfaceSize() const noexcept {
        return PixelDensityCap::apply(static_cast<int>(windowSize.x), static_cast<int>(windowSize.y), dpiScale, maxPixelDensity);
    }
    [[nodiscard]] float getMaxPixelDensity() const noexcept override {
        return maxPixelDensity;
    }
    void setMaxPixelDensity(float value) override {
        requireMaxPixelDensity(value);
        maxPixelDensity = value;
        followDrawable();
    }
    [[nodiscard]] bool isFullscreen() const noexcept override {
        return fullscreen;
    }
    void setFullscreen(bool value, std::function<void(Permission::Answer answer)> callback) override {
        if (windowAnswer.state == Permission::State::Granted) {
            fullscreen = value;
            fullscreenTarget = {};
        }
        callback(windowAnswer);
    }

    // Goes fullscreen like a desktop, on the display and in the mode of the target, which `getFullscreenTarget` reports.
    void enterFullscreen(const Fullscreen& value, std::function<void(Permission::Answer answer)> callback) override;
    [[nodiscard]] const Fullscreen& getFullscreenTarget() const noexcept {
        return fullscreenTarget;
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
    void setMouseLocked(bool value, std::function<void(Permission::Answer answer)> callback) override {
        if (windowAnswer.state == Permission::State::Granted) {
            mouseLocked = value;
        }
        callback(windowAnswer);
    }
    void setKeyboardVisible(bool value) override {
        keyboardVisible = value;
    }
    // The clipboard keeps the text unless the test refuses it, the way a browser refuses a page, and answers at once.
    void setClipboard(std::string_view value, std::function<void(Permission::Answer answer)> callback) override {
        if (clipboardAnswer.state == Permission::State::Granted) {
            clipboard = value;
        }
        callback(clipboardAnswer);
    }
    [[nodiscard]] std::string getClipboard() const override {
        return clipboard;
    }
    void readClipboard(std::function<void(Permission::Answer answer, std::string text)> callback) override {
        callback(clipboardAnswer, clipboardAnswer.state == Permission::State::Granted ? clipboard : std::string());
    }
    void requestQuit() override {
        quitRequested = true;
    }
    [[nodiscard]] bool canQuit() const noexcept override {
        return true;
    }

    // The headless screen turns with its framebuffer, like the screen of a phone.
    [[nodiscard]] Orientation getOrientation() const override {
        return windowSize.y > windowSize.x ? Orientation::Portrait : Orientation::Landscape;
    }
    void lockOrientation(Orientation value) override {
        orientationLock = value;
    }

    // The headless screen has no fold until a test gives it one, as a foldable device reports it.
    void getCutouts(std::vector<math::Rect>& cutouts) const override {
        cutouts.assign(screenCutouts.begin(), screenCutouts.end());
    }

    // The cutouts of the screen that the headless window reports, in framebuffer pixels.
    void setCutouts(std::vector<math::Rect> value) {
        screenCutouts = std::move(value);
    }

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
    [[nodiscard]] SystemGestures getSystemGestures() const noexcept override {
        return systemGestures;
    }
    void setSystemGestures(SystemGestures value) override {
        systemGestures = value;
    }
    [[nodiscard]] HeadlessTextInput& getTextInput() noexcept override {
        return textInput;
    }
    [[nodiscard]] bool hasPointerDevice() const noexcept override {
        return pointerDevice;
    }
    [[nodiscard]] bool isScreenKeptOn() const noexcept override {
        return screenKeptOn;
    }
    void setKeepScreenOn(bool value, std::function<void(Permission::Answer answer)> callback) override {
        if (windowAnswer.state == Permission::State::Granted) {
            screenKeptOn = value;
        }
        callback(windowAnswer);
    }
    [[nodiscard]] int getFrameRate() const noexcept override {
        return frameRate;
    }
    void setFrameRate(int value) override {
        requireFrameRate(value);
        frameRate = value;
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
    [[nodiscard]] std::vector<Display> getDisplays() const override {
        return displays;
    }

    // The window is on the display that holds most of its frame, like a desktop window.
    [[nodiscard]] std::string getDisplayId() const override {
        return Display::findHolding(displays, frame).id;
    }

    // Outputs open on the headless platform as the requests record them, and tests report them with `setOutputReport` and the event `OutputChanged`.
    void openOutput(std::uint32_t number, std::string_view display, const Output::Options& options) override {
        outputRequests.push_back({.number = number, .display = std::string(display), .options = options});
        outputReports[number] = {};
    }
    void closeOutput(std::uint32_t number) override {
        outputReports.erase(number);
        closedOutputs.push_back(number);
        surfacesAtClose.push_back(gpu != nullptr ? gpu->getAddedSurfaces().size() : 0);
    }
    [[nodiscard]] OutputReport getOutputReport(std::uint32_t number) const override {
        return outputReports.at(number);
    }

    [[nodiscard]] std::string_view getPlatformName() const noexcept override {
        return "headless";
    }
    [[nodiscard]] std::unique_ptr<gpu::Device> createGpuDevice() override;

    // The null device of the app that runs, which tests read what the frames did from, while the host draws through it.
    [[nodiscard]] gpu::NullDevice& getGpu() const {
        return *gpu;
    }

    // Makes the devices of the apps that run instead of the null device, such as a real device that draws offscreen in the GPU tests.
    void setGpuDeviceMaker(std::function<std::unique_ptr<gpu::Device>()> maker) {
        gpuDeviceMaker = std::move(maker);
    }
    // Without an output backend the mixer has no device and mixes only when a test asks it to.
    [[nodiscard]] audio::Mixer::Setup getAudioSetup() const override {
        return {.device = audioOutput != nullptr, .keepsTime = true, .backend = audioOutput};
    }

    // Gives the mixers of the engines created afterwards a device that opens through the backend, which simulates an audio output that plays, refuses or never answers.
    void setAudioOutput(const audio::OutputBackend* value) noexcept {
        audioOutput = value;
    }
    [[nodiscard]] bool isFrameTransparent() const noexcept override;
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
    void setGamepadMotion(input::GamepadMotion::Sensors sensors) override {
        gamepadMotion = sensors;
    }
    void dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views) override;
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
    [[nodiscard]] std::optional<bool> getPowerSaving() const override {
        return systemState.getPowerSaving();
    }
    [[nodiscard]] std::string getTimeZone() const override {
        return systemState.getTimeZone();
    }
    [[nodiscard]] std::vector<std::string> getLanguages() const override {
        return systemState.getLanguages();
    }
    [[nodiscard]] std::uint64_t getLanguagesRevision() const override {
        return systemState.getLanguagesRevision();
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
    void setBackgroundAudio(bool value) override {
        backgroundAudio = value;
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
    void rumbleGamepad(std::size_t index, float low, float high, float seconds) override {
        rumbles.push_back({.index = index, .low = low, .high = high, .seconds = seconds});
    }
    void checkPermission(Permission::Kind, std::function<void(Permission::Answer answer)> callback) override {
        callback(permissionState);
    }
    void requestPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) override;
    // Every socket of the headless platform reads the reason a test set, the way a platform explains a socket it blocked.
    [[nodiscard]] BlockedReason getBlockedReason() const override {
        return [reason = blockedReason](std::intptr_t) { return reason; };
    }
    [[nodiscard]] std::string getNetworkRequirement() const override {
        return networkRequirement;
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
    [[nodiscard]] bool canHaltFrames() const noexcept override {
        return haltable;
    }
    // No person watches a headless app, so a halt whose client never lets it go on ends at a bound instead of holding a test forever.
    [[nodiscard]] bool pumpHaltedEvents() override {
        ++haltedPumps;
        return finishingHalts || haltedPumps > kMaxHaltedPumps;
    }

    // Whether the debugger may halt the frames, which a test turns off to play a web page, and whether the platform asks a halted frame to finish, as a quit does.
    void setHaltable(bool value) noexcept {
        haltable = value;
    }
    void setFinishingHalts(bool value) noexcept {
        finishingHalts = value;
    }

    // How often the debugger kept the platform responsive while it halted.
    [[nodiscard]] int getHaltedPumps() const noexcept {
        return haltedPumps;
    }

    [[nodiscard]] bool isLaunched() const noexcept override {
        return launched;
    }
    void setLaunched(bool value) noexcept {
        launched = value;
    }
    [[nodiscard]] bool hasNativeAccess() const noexcept override {
        return nativeAccess;
    }
    void setNativeAccess(bool value) noexcept {
        nativeAccess = value;
    }
    [[nodiscard]] bool isBackHeld() const noexcept override {
        return backHeld;
    }
    void setBackHeld(bool value) noexcept {
        backHeld = value;
    }

    // Plays the apps of this host in development from now on, with a session that scans the package folder when one is given, the way the player does with "--dev", and that connects to a development server when its address is given.
    void enableDevelopment(std::optional<std::filesystem::path> folder = std::nullopt, std::string server = {}) {
        developmentSession = std::make_unique<DevelopmentSession>(*this, std::move(folder), std::move(server));
    }

    // Takes the development session of the next app and hands back the one of the app before it, such as when a launched app plays with a session of its own, or with none, while the app that launched it keeps its own for its return.
    [[nodiscard]] std::unique_ptr<DevelopmentSession> exchangeDevelopmentSession(std::unique_ptr<DevelopmentSession> value) noexcept {
        return std::exchange(developmentSession, std::move(value));
    }

    // Resizes the window in pixels, whose drawable the pixel density cap may shrink.
    void resize(math::Vec2 size) noexcept {
        windowSize = size;
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
    void setDisplays(std::vector<Display> value) {
        displays = std::move(value);
    }

    // An output that the app asked the headless platform to open.
    struct OutputRequest {
        std::uint32_t number = 0;
        std::string display;
        Output::Options options;
    };

    [[nodiscard]] const std::vector<OutputRequest>& getOutputRequests() const noexcept {
        return outputRequests;
    }
    [[nodiscard]] const std::vector<std::uint32_t>& getClosedOutputs() const noexcept {
        return closedOutputs;
    }

    // The surfaces beyond the window that the null device still had as each output closed.
    [[nodiscard]] const std::vector<std::size_t>& getSurfacesAtClose() const noexcept {
        return surfacesAtClose;
    }
    void setOutputReport(std::uint32_t number, OutputReport value) {
        outputReports[number] = std::move(value);
    }

    // How the headless window answers fullscreen, the lock of the pointer and keeping the screen on, at once, the way a browser refuses them without an input of the person.
    void setWindowAnswer(Permission::Answer value) {
        windowAnswer = std::move(value);
    }

    // The ids of the plugins whose native part the headless platform reports as loaded.
    void setNativePlugins(std::vector<std::string> value) {
        nativePlugins = std::move(value);
    }

    // What the headless system reports to the apps that start after the change. It starts as a desktop of the operating system the tests run on.
    void setSystemInfo(SystemInfo value) {
        systemInfo = std::move(value);
    }

    // Change the theme, the battery, the power saving, the time zone, the accessibility settings and the languages from any thread, the way platform services report them. The engine publishes the change at its next frame.
    void setTheme(Theme value) {
        systemState.setTheme(value);
    }
    void setBattery(const Battery& value) {
        systemState.setBattery(value);
    }
    void setPowerSaving(std::optional<bool> value) {
        systemState.setPowerSaving(value);
    }
    void setTimeZone(std::string value) {
        systemState.setTimeZone(std::move(value));
    }
    void setAccessibilitySettings(const AccessibilitySettings& value) {
        systemState.setAccessibility(value);
    }
    void setLanguages(std::span<const std::string> value) {
        systemState.setLanguages(value);
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

    // Whether the app that started last plays audio in the background, as it told the platform.
    [[nodiscard]] bool isBackgroundAudio() const noexcept {
        return backgroundAudio;
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

    // The answer of every write and read of the clipboard, which a browser or the person may refuse on a real device.
    void setClipboardAnswer(Permission::Answer value) {
        clipboardAnswer = std::move(value);
    }

    // The answer of every check of a permission and of every request for one, which the platform and the person give on a real device.
    void setPermission(Permission::Answer value) {
        permissionState = std::move(value);
    }
    void setPermissionAnswer(Permission::Answer value) {
        permissionAnswer = std::move(value);
    }

    // Holds the answers of the requests for permissions until `answerPermissions`, the way a person takes a while to answer the prompt of the system.
    void setPermissionAnswersHeld(bool value) noexcept {
        permissionAnswersHeld = value;
    }
    void answerPermissions();
    [[nodiscard]] const std::vector<Permission::Kind>& getPermissionRequests() const noexcept {
        return permissionRequests;
    }

    // The motion sensors that run now on the gamepads of the headless host.
    [[nodiscard]] input::GamepadMotion::Sensors getGamepadMotion() const noexcept {
        return gamepadMotion;
    }

    // What the platform explains for a socket of the connections that start after the change, such as a connection that Local Network Protection of Android blocked.
    void setBlockedReason(std::string value) {
        blockedReason = std::move(value);
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

    [[nodiscard]] const std::vector<Rumble>& getRumbles() const noexcept {
        return rumbles;
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
    void followDrawable();

    gpu::NullDevice* gpu = nullptr;
    std::function<std::unique_ptr<gpu::Device>()> gpuDeviceMaker;
    std::filesystem::path dataDirectory;
    math::Vec2 windowSize;
    float dpiScale = 1.0F;
    float maxPixelDensity = 0.0F;
    math::Insets safeAreaInsets{};
    StatusBar statusBar = StatusBar::Hidden;
    SystemGestures systemGestures = SystemGestures::Deferred;
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
    std::vector<Rumble> rumbles;
    std::string networkRequirement;
    Permission::Answer permissionState{.state = Permission::State::Granted};
    Permission::Answer permissionAnswer{.state = Permission::State::Granted};
    std::vector<Permission::Kind> permissionRequests;
    std::vector<std::function<void(Permission::Answer)>> heldPermissionAnswers;
    bool permissionAnswersHeld = false;
    input::GamepadMotion::Sensors gamepadMotion;
    std::string blockedReason;
    std::vector<AccessibilityTree> accessibilityTrees;
    std::vector<Announcement> announcements;
    std::optional<MediaSession::NowPlaying> nowPlaying;
    bool backgroundAudio = false;
    bool accessibilityWanted = false;
    VideoOpener videoOpener;
    std::vector<ScreenRequest> screenRequests;
    std::vector<std::uint64_t> cancelledScreens;
    std::filesystem::path crashReport;
    std::string crashContext;
    static constexpr int kMaxHaltedPumps = 2000;

    std::unique_ptr<DevelopmentSession> developmentSession;
    bool haltable = true;
    bool finishingHalts = false;
    int haltedPumps = 0;
    bool launched = false;
    bool nativeAccess = true;
    bool backHeld = false;
    const audio::OutputBackend* audioOutput = nullptr;
    Permission::Answer clipboardAnswer{.state = Permission::State::Granted};

    // The headless desktop is one 1920 by 1080 display at 60 frames per second whose work area leaves a 40 point taskbar at the bottom, with the window at the top left corner of it.
    std::vector<Display> displays{{.id = "headless", .name = "headless", .bounds = {0.0F, 0.0F, 1920.0F, 1080.0F}, .workArea = {0.0F, 0.0F, 1920.0F, 1040.0F}, .scale = 1.0F, .refreshRate = 60.0F, .primary = true, .modes = {{.width = 1920, .height = 1080, .refreshRate = 60.0F, .scale = 1.0F}, {.width = 1280, .height = 720, .refreshRate = 60.0F, .scale = 1.0F}}}};
    Permission::Answer windowAnswer{.state = Permission::State::Granted};
    std::vector<math::Rect> screenCutouts;
    Fullscreen fullscreenTarget;
    std::vector<OutputRequest> outputRequests;
    std::vector<std::uint32_t> closedOutputs;
    std::vector<std::size_t> surfacesAtClose;
    std::unordered_map<std::uint32_t, OutputReport> outputReports;
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
    bool screenKeptOn = false;
    int frameRate = 0;
    bool transparencySupported = false;
    bool transparent = false;
    bool decorated = true;
    bool alwaysOnTop = false;
    bool shownInTaskbar = true;
    bool focusable = true;
};

} // namespace haylen::platform
