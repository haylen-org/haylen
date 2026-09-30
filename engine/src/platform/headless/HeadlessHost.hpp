#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "haylen/input/Input.hpp"
#include "haylen/math/Insets.hpp"
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
        return 1.0F;
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
    [[nodiscard]] audio::Mixer::Setup getAudioSetup() const override {
        return {.device = false};
    }
    [[nodiscard]] graphics::FrameTarget getFrameTarget() override;
    [[nodiscard]] std::filesystem::path getUserDataDirectory(std::string_view identifier) override;
    void persistUserData() override {
        ++persistCount;
    }
    [[nodiscard]] math::Insets getSafeAreaInsets() const override {
        return safeAreaInsets;
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
    void openUrl(std::string_view url, std::function<void(bool opened)> callback) override;
    void vibrate(float seconds) override {
        vibrations.push_back(seconds);
    }
    void showDialog(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder) override {
        dialogCalls.push_back({.id = id, .request = request, .folder = folder});
    }
    void cancelDialog(std::uint64_t id) override {
        cancelledDialogs.push_back(id);
    }

    void resize(math::Vec2 size) noexcept {
        framebufferSize = size;
    }
    void setSafeAreaInsets(const math::Insets& value) noexcept {
        safeAreaInsets = value;
    }
    void setGamepad(std::size_t index, input::GamepadState state) {
        gamepads.at(index) = std::move(state);
    }
    void setPointerDevice(bool value) noexcept {
        pointerDevice = value;
    }

    // Opens the headless window able to be transparent, and transparent, the way window.transparent of app.json opens a real one.
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

    // Change the theme and the battery from any thread, the way platform services report them. The engine publishes the change at its next frame.
    void setTheme(Theme value) {
        systemState.setTheme(value);
    }
    void setBattery(const Battery& value) {
        systemState.setBattery(value);
    }

    // Whether the headless system finds an app for the urls the engine opens, which it answers at once.
    void setOpensUrls(bool value) noexcept {
        opensUrls = value;
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
    [[nodiscard]] const std::vector<float>& getVibrations() const noexcept {
        return vibrations;
    }
    [[nodiscard]] const std::vector<DialogCall>& getDialogRequests() const noexcept {
        return dialogCalls;
    }
    [[nodiscard]] const std::vector<std::uint64_t>& getCancelledDialogs() const noexcept {
        return cancelledDialogs;
    }

  private:
    std::filesystem::path dataDirectory;
    math::Vec2 framebufferSize;
    math::Insets safeAreaInsets{};
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
    std::vector<DialogCall> dialogCalls;
    std::vector<std::uint64_t> cancelledDialogs;
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
