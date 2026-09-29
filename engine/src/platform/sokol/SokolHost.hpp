#pragma once

#include <cstdint>
#include <vector>

#include "haylen/math/Insets.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/Host.hpp"
#include "platform/KeyboardTranslator.hpp"
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

    [[nodiscard]] std::string_view getPlatformName() const noexcept override;
    [[nodiscard]] graphics::DeviceSetup getGraphicsSetup() override;
    [[nodiscard]] audio::Mixer::Setup getAudioSetup() const override;
    [[nodiscard]] graphics::FrameTarget getFrameTarget() override;
    [[nodiscard]] std::filesystem::path getUserDataDirectory(std::string_view identifier) override;
    void persistUserData() override;
    [[nodiscard]] math::Insets getSafeAreaInsets() const override;
    void pollGamepads(std::span<input::GamepadState> gamepads) override;
    void dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson) override;

    // Turns the edits and actions of the plain keyboard that setKeyboardVisible opens into key and character events.
    [[nodiscard]] std::vector<Event> translateKeyboard(const Event& event) {
        return keyboard.translate(event);
    }

  private:
    [[nodiscard]] static sapp_mouse_cursor toSokolCursor(Cursor cursor) noexcept;

    KeyboardTranslator keyboard;
    std::uint64_t keyboardRevision = 0;
    bool resizable = true;
};

} // namespace haylen::platform
