#pragma once

#include <cstdint>
#include <vector>

#include "haylen/math/Insets.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/Host.hpp"
#include "platform/KeyboardTranslator.hpp"
#include "platform/NativeViews.hpp"
#include "platform/SystemState.hpp"
#include "platform/WindowStyle.hpp"
#include "sokol_app.h"

namespace haylen::platform {

// Host backed by sokol_app, sokol_glue and the services of the platform folder compiled into the runtime.
class SokolHost final : public Host {
  public:
    [[nodiscard]] math::Vec2 getFramebufferSize() const noexcept override;
    [[nodiscard]] float getDpiScale() const noexcept override;
    [[nodiscard]] bool isFullscreen() const noexcept override;
    void setFullscreen(bool value) override;
    [[nodiscard]] bool isResizable() const noexcept override;
    void setResizable(bool value) override;
    void setTitle(std::string_view value) override;
    void setCursor(Cursor value) override;
    void setCursorVisible(bool value) override;
    void setMouseLocked(bool value) override;
    void setKeyboardVisible(bool value) override;
    void setClipboard(std::string_view value) override;
    [[nodiscard]] std::string getClipboard() const override;
    void requestQuit() override;
    [[nodiscard]] Orientation getOrientation() const override;
    void lockOrientation(Orientation value) override;
    [[nodiscard]] TextInput& getTextInput() noexcept override;
    [[nodiscard]] bool hasPointerDevice() const noexcept override;
    [[nodiscard]] bool canBeTransparent() const noexcept override;
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
    [[nodiscard]] graphics::DeviceSetup getGraphicsSetup() override;
    [[nodiscard]] audio::Mixer::Setup getAudioSetup() const override;
    [[nodiscard]] graphics::FrameTarget getFrameTarget() override;
    [[nodiscard]] std::filesystem::path getUserDataDirectory(std::string_view identifier) override;
    void persistUserData() override;
    [[nodiscard]] math::Insets getSafeAreaInsets() const override;
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
    void openUrl(std::string_view url, std::function<void(bool opened)> callback) override;
    void vibrate(float seconds) override;
    void showDialog(std::uint64_t id, const DialogRequest& request, const std::filesystem::path& folder) override;
    void cancelDialog(std::uint64_t id) override;

    // The native views of plugins over the app, which the platform services update from any thread. They belong to the process, so a reservation or a cover outlives the apps that restart under it.
    [[nodiscard]] static NativeViews& getNativeViews() noexcept;

    // The theme and the battery that the platform services report from any thread, at initialize and whenever the system changes them. They belong to the process, so every app that starts hears the last report.
    [[nodiscard]] static SystemState& getSystemState() noexcept;

    // Records the options the window opens with, before it exists, so the options the engine applies when the app starts find the window as it already is.
    void prepare(const WindowStyle& openingStyle, bool openingFocusable) noexcept {
        style = openingStyle;
        focusable = openingFocusable;
    }

    // Turns the edits and actions of the plain keyboard that setKeyboardVisible opens into key and character events.
    [[nodiscard]] std::vector<Event> translateKeyboard(const Event& event) {
        return keyboard.translate(event);
    }

  private:
    [[nodiscard]] static sapp_mouse_cursor toSokolCursor(Cursor cursor) noexcept;

    // Fullscreen windows keep the style of the platform, and leaving fullscreen brings this one back.
    void applyStyle();

    KeyboardTranslator keyboard;
    std::uint64_t keyboardRevision = 0;
    WindowStyle style;
    Passthrough passthrough = Passthrough::Off;
    bool focusable = true;
};

} // namespace haylen::platform
