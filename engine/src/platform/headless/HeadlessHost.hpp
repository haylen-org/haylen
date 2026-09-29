#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "haylen/input/Input.hpp"
#include "haylen/math/Insets.hpp"
#include "platform/Host.hpp"
#include "platform/headless/HeadlessTextInput.hpp"

namespace haylen::platform {

// Host without a window or GPU. It drives the engine with the Sokol dummy backend so rendering, Lua and the frame loop run in tests.
class HeadlessHost final : public Host {
  public:
    struct PlatformCall {
        std::uint64_t id = 0;
        std::string method;
        std::string paramsJson;
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
    void dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson) override;

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

  private:
    std::filesystem::path dataDirectory;
    math::Vec2 framebufferSize;
    math::Insets safeAreaInsets{};
    std::array<input::GamepadState, input::Input::kMaxGamepads> gamepads{};
    std::vector<PlatformCall> platformCalls;
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
};

} // namespace haylen::platform
