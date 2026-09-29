#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "haylen/input/Controls.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/input/InputDevice.hpp"
#include "haylen/input/KeyModifiers.hpp"
#include "haylen/input/Touch.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::platform {
struct Event;
}

namespace haylen::graphics {
class Viewport;
}

namespace haylen::input {

// Raw device state for the current frame. Positions are in design coordinates.
class Input final {
  public:
    static constexpr std::size_t kMaxGamepads = 4;

    void handleEvent(const platform::Event& event, const graphics::Viewport& viewport);
    void updateGamepads(std::span<const GamepadState> states);
    void updateTouchDurations(float deltaSeconds) noexcept;
    void endFrame();

    // Releases every held key and mouse button and cancels every touch, which the engine does when the app loses focus or leaves the foreground.
    void releaseAll() noexcept;

    // Keeps the pointer and the fingers on the same screen points when the app changes how design space maps onto the screen.
    void followViewport(const graphics::Viewport& previous, const graphics::Viewport& viewport) noexcept;

    [[nodiscard]] bool isKeyDown(Key key) const noexcept;
    [[nodiscard]] bool isKeyPressed(Key key) const noexcept;
    [[nodiscard]] bool isKeyReleased(Key key) const noexcept;
    [[nodiscard]] bool isAnyKeyPressed() const noexcept {
        return keysPressed.any();
    }
    [[nodiscard]] KeyModifiers getModifiers() const noexcept {
        return modifiers;
    }
    [[nodiscard]] std::span<const char32_t> getText() const noexcept {
        return text;
    }

    [[nodiscard]] bool isMouseDown(MouseButton button) const noexcept;
    [[nodiscard]] bool isMousePressed(MouseButton button) const noexcept;
    [[nodiscard]] bool isMouseReleased(MouseButton button) const noexcept;
    [[nodiscard]] math::Vec2 getMousePosition() const noexcept {
        return mousePosition;
    }
    [[nodiscard]] math::Vec2 getMouseFramebufferPosition() const noexcept {
        return mouseFramebufferPosition;
    }
    [[nodiscard]] math::Vec2 getMouseDelta() const noexcept {
        return mouseDelta;
    }
    [[nodiscard]] math::Vec2 getMouseScroll() const noexcept {
        return mouseScroll;
    }
    [[nodiscard]] bool isMouseInside() const noexcept {
        return mouseInside;
    }

    // The UI captures the pointer while it is over something the interface owns, and the action map ignores mouse buttons meanwhile.
    void setPointerCaptured(bool value) noexcept {
        pointerCaptured = value;
    }
    [[nodiscard]] bool isPointerCaptured() const noexcept {
        return pointerCaptured;
    }

    [[nodiscard]] std::span<const Touch> getTouches() const noexcept {
        return touches;
    }
    [[nodiscard]] const Touch* findTouch(std::uint64_t id) const noexcept;

    [[nodiscard]] const GamepadState& getGamepad(std::size_t index) const noexcept {
        return gamepads[index];
    }
    [[nodiscard]] bool isGamepadDown(std::size_t index, GamepadButton button) const noexcept;
    [[nodiscard]] bool isGamepadPressed(std::size_t index, GamepadButton button) const noexcept;
    [[nodiscard]] bool isGamepadReleased(std::size_t index, GamepadButton button) const noexcept;
    [[nodiscard]] float getGamepadAxis(std::size_t index, GamepadAxis axis) const noexcept;
    [[nodiscard]] math::Vec2 getGamepadStick(std::size_t index, bool rightStick) const noexcept;

    void setGamepadDeadzone(float value) noexcept {
        deadzone = value;
    }
    [[nodiscard]] float getGamepadDeadzone() const noexcept {
        return deadzone;
    }
    [[nodiscard]] InputDevice getLastDevice() const noexcept {
        return lastDevice;
    }

  private:
    [[nodiscard]] static std::size_t keyIndex(Key key) noexcept;
    [[nodiscard]] static std::size_t buttonIndex(MouseButton button) noexcept;
    [[nodiscard]] static float applyDeadzone(float value, float threshold) noexcept;

    void handleTouches(const platform::Event& event, const graphics::Viewport& viewport);

    std::bitset<Controls::kKeyCount> keysDown;
    std::bitset<Controls::kKeyCount> keysPressed;
    std::bitset<Controls::kKeyCount> keysReleased;
    std::bitset<Controls::kMouseButtonCount> mouseDown;
    std::bitset<Controls::kMouseButtonCount> mousePressed;
    std::bitset<Controls::kMouseButtonCount> mouseReleased;
    std::vector<char32_t> text;
    std::vector<Touch> touches;
    std::array<GamepadState, kMaxGamepads> gamepads{};
    std::array<GamepadState, kMaxGamepads> previousGamepads{};
    KeyModifiers modifiers{};
    math::Vec2 mousePosition{};
    math::Vec2 mouseFramebufferPosition{};
    math::Vec2 mouseDelta{};
    math::Vec2 mouseScroll{};
    float deadzone = 0.2F;
    bool mouseInside = false;
    bool pointerCaptured = false;
    InputDevice lastDevice = InputDevice::KeyboardMouse;
};

} // namespace haylen::input
