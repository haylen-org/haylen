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
#include "haylen/input/MotionState.hpp"
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
    static constexpr std::size_t kMaxGamepads = 8;
    // A precise scroll counts as one step of a wheel for every this many points of the screen it moves.
    static constexpr float kPointsPerLine = 40.0F;

    void handleEvent(const platform::Event& event, const graphics::Viewport& viewport);
    void updateGamepads(std::span<const GamepadState> states);
    void updateMotion(const MotionState& state) noexcept {
        motion = state;
    }
    void updateTouchDurations(float deltaSeconds) noexcept;
    void endFrame();

    // Releases every held key, modifier and mouse button and cancels every touch, which the engine does when the app loses focus or leaves the foreground.
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
    // The scroll of the frame in steps of a wheel, where precise scrolls count `kPointsPerLine` points as a step.
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

    [[nodiscard]] const GamepadState& getGamepad(std::size_t index) const noexcept;
    [[nodiscard]] bool isGamepadDown(std::size_t index, GamepadButton button) const noexcept;
    [[nodiscard]] bool isGamepadPressed(std::size_t index, GamepadButton button) const noexcept;
    [[nodiscard]] bool isGamepadReleased(std::size_t index, GamepadButton button) const noexcept;
    [[nodiscard]] float getGamepadAxis(std::size_t index, GamepadAxis axis) const noexcept;
    [[nodiscard]] math::Vec2 getGamepadStick(std::size_t index, bool rightStick) const noexcept;

    // The readings of the motion sensors of the device that run.
    [[nodiscard]] const MotionState& getMotion() const noexcept {
        return motion;
    }

    // How far the device leans from level, read from the gravity that its accelerometer measures, as a vector up to 1 that follows design space like a stick: x grows as the right edge of the screen goes down and y as the bottom edge goes down, so a ball that rolls with it moves by the vector times gravity. It is zero while the accelerometer of the device does not run.
    [[nodiscard]] math::Vec2 getTilt() const noexcept;

    // The same tilt for a gamepad, from its face lying flat, while its accelerometer runs.
    [[nodiscard]] math::Vec2 getGamepadTilt(std::size_t index) const noexcept;

    // Takes the way the device leans now as level, so the tilt is zero while the person keeps holding it that way, and returns false without a reading of the accelerometer. A device that lies flat face up is level until the first calibration and again after `resetTilt`.
    bool calibrateTilt() noexcept;
    void resetTilt() noexcept;

    // Takes a dead zone from 0 to below 1 and throws `std::invalid_argument` for any other value.
    void setGamepadDeadzone(float value);
    [[nodiscard]] float getGamepadDeadzone() const noexcept {
        return deadzone;
    }

    // Multiplies every scroll of the wheels and the trackpads before the app and the UI see it, 1 by default. Throws `std::invalid_argument` for a value that is not positive and finite.
    void setScrollSpeed(float value);
    [[nodiscard]] float getScrollSpeed() const noexcept {
        return scrollSpeed;
    }
    [[nodiscard]] InputDevice getLastDevice() const noexcept {
        return lastDevice;
    }

  private:
    static const GamepadState kDisconnectedGamepad;

    [[nodiscard]] static std::size_t keyIndex(Key key) noexcept;
    [[nodiscard]] static std::size_t buttonIndex(MouseButton button) noexcept;
    [[nodiscard]] static float applyDeadzone(float value, float threshold) noexcept;

    // Turns the gravity of an acceleration so that the level direction points out of the screen, and reads how far it leans from there.
    [[nodiscard]] static math::Vec2 measureTilt(const std::array<float, 3>& acceleration, const std::array<float, 3>& level) noexcept;

    void handleTouches(const platform::Event& event, const graphics::Viewport& viewport);
    [[nodiscard]] bool hasGamepadPress(std::size_t index) const noexcept;

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
    MotionState motion;
    std::array<float, 3> tiltLevel{0.0F, 0.0F, 1.0F};
    KeyModifiers modifiers{};
    math::Vec2 mousePosition{};
    math::Vec2 mouseFramebufferPosition{};
    math::Vec2 mouseDelta{};
    math::Vec2 mouseScroll{};
    float deadzone = 0.2F;
    float scrollSpeed = 1.0F;
    bool mouseInside = false;
    bool pointerCaptured = false;
    InputDevice lastDevice = InputDevice::KeyboardMouse;
};

} // namespace haylen::input
