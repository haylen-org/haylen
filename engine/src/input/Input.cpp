#include "haylen/input/Input.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::input {

const GamepadState Input::kDisconnectedGamepad{};

std::size_t Input::keyIndex(Key key) noexcept {
    return static_cast<std::size_t>(key);
}

std::size_t Input::buttonIndex(MouseButton button) noexcept {
    return static_cast<std::size_t>(button);
}

float Input::applyDeadzone(float value, float threshold) noexcept {
    const float magnitude = std::fabs(value);
    if (magnitude <= threshold) {
        return 0.0F;
    }
    return std::copysign((magnitude - threshold) / (1.0F - threshold), value);
}

void Input::handleEvent(const platform::Event& event, const graphics::Viewport& viewport) {
    switch (event.type) {
    case platform::Event::Type::KeyDown:
        if (keyIndex(event.key) < Controls::kKeyCount && !event.repeat) {
            keysDown.set(keyIndex(event.key));
            keysPressed.set(keyIndex(event.key));
        }
        modifiers = event.modifiers;
        if (!event.repeat) {
            lastDevice = InputDevice::KeyboardMouse;
        }
        break;
    case platform::Event::Type::KeyUp:
        if (keyIndex(event.key) < Controls::kKeyCount) {
            keysDown.reset(keyIndex(event.key));
            keysReleased.set(keyIndex(event.key));
        }
        modifiers = event.modifiers;
        break;
    case platform::Event::Type::Character:
        text.push_back(event.character);
        break;
    case platform::Event::Type::MouseDown:
        mouseDown.set(buttonIndex(event.mouseButton));
        mousePressed.set(buttonIndex(event.mouseButton));
        mouseFramebufferPosition = event.position;
        mousePosition = viewport.toDesign(event.position);
        lastDevice = InputDevice::KeyboardMouse;
        break;
    case platform::Event::Type::MouseUp:
        mouseDown.reset(buttonIndex(event.mouseButton));
        mouseReleased.set(buttonIndex(event.mouseButton));
        mouseFramebufferPosition = event.position;
        mousePosition = viewport.toDesign(event.position);
        break;
    case platform::Event::Type::MouseMove: {
        const math::Vec2 previous = mousePosition;
        mouseFramebufferPosition = event.position;
        mousePosition = viewport.toDesign(event.position);
        mouseDelta += event.delta.isZero() ? mousePosition - previous : event.delta / viewport.getPixelsPerUnit();
        mouseInside = true;
        break;
    }
    case platform::Event::Type::MouseScroll:
        mouseScroll += event.scroll;
        break;
    case platform::Event::Type::MouseEnter:
        mouseInside = true;
        break;
    case platform::Event::Type::MouseLeave:
        mouseInside = false;
        break;
    case platform::Event::Type::TouchBegan:
    case platform::Event::Type::TouchMoved:
    case platform::Event::Type::TouchEnded:
    case platform::Event::Type::TouchCancelled:
        handleTouches(event, viewport);
        lastDevice = InputDevice::Touch;
        break;
    default:
        break;
    }
}

void Input::handleTouches(const platform::Event& event, const graphics::Viewport& viewport) {
    for (std::size_t index = 0; index < event.touchCount; ++index) {
        const platform::TouchPoint& point = event.touches[index];
        if (!point.changed) {
            continue;
        }

        // A platform may give a new finger the id of a touch that ended this frame, so only a touch that is still down answers to an id.
        const math::Vec2 position = viewport.toDesign(point.position);
        const auto found = std::find_if(touches.begin(), touches.end(), [&point](const Touch& touch) { return touch.id == point.id && touch.isActive(); });

        if (event.type == platform::Event::Type::TouchBegan) {
            if (found == touches.end()) {
                touches.push_back({.id = point.id, .position = position, .startPosition = position, .previousPosition = position});
            }
            continue;
        }

        if (found == touches.end()) {
            continue;
        }

        // The previous position stays where the frame started, so every move of the frame adds up in the delta.
        found->position = position;
        if (event.type == platform::Event::Type::TouchMoved) {
            found->phase = found->phase == TouchPhase::Began ? TouchPhase::Began : TouchPhase::Moved;
        } else {
            found->phase = event.type == platform::Event::Type::TouchEnded ? TouchPhase::Ended : TouchPhase::Cancelled;
        }
    }
}

void Input::updateGamepads(std::span<const GamepadState> states) {
    // The current states become the previous ones by swapping, and a name is copied only when the gamepad in a slot changes, so an ordinary frame copies no strings.
    previousGamepads.swap(gamepads);
    for (std::size_t index = 0; index < kMaxGamepads; ++index) {
        const GamepadState& source = index < states.size() ? states[index] : kDisconnectedGamepad;
        GamepadState& state = gamepads[index];
        state.connected = source.connected;
        state.buttons = source.buttons;
        state.axes = source.axes;
        if (state.name != source.name) {
            state.name = source.name;
        }
    }

    // Only a press makes the gamepad the last used device, so a button or stick held meanwhile never takes over from a later key press or touch.
    for (std::size_t index = 0; index < kMaxGamepads; ++index) {
        if (gamepads[index].connected && hasGamepadPress(index)) {
            lastDevice = InputDevice::Gamepad;
        }
    }
}

bool Input::hasGamepadPress(std::size_t index) const noexcept {
    const GamepadState& state = gamepads[index];
    const GamepadState& previous = previousGamepads[index];
    for (std::size_t slot = 0; slot < Controls::kGamepadButtonCount; ++slot) {
        if (state.buttons[slot] && !previous.buttons[slot]) {
            return true;
        }
    }
    for (std::size_t slot = 0; slot < Controls::kGamepadAxisCount; ++slot) {
        if (std::fabs(state.axes[slot]) > deadzone && std::fabs(previous.axes[slot]) <= deadzone) {
            return true;
        }
    }
    return false;
}

void Input::updateTouchDurations(float deltaSeconds) noexcept {
    for (Touch& touch : touches) {
        touch.duration += deltaSeconds;
    }
}

void Input::releaseAll() noexcept {
    keysReleased |= keysDown;
    keysDown.reset();
    mouseReleased |= mouseDown;
    mouseDown.reset();
    modifiers = {};
    for (Touch& touch : touches) {
        touch.phase = TouchPhase::Cancelled;
    }
}

void Input::followViewport(const graphics::Viewport& previous, const graphics::Viewport& viewport) noexcept {
    mousePosition = viewport.toDesign(mouseFramebufferPosition);
    for (Touch& touch : touches) {
        for (math::Vec2* point : {&touch.position, &touch.startPosition, &touch.previousPosition}) {
            *point = viewport.toDesign(previous.toFramebuffer(*point));
        }
    }
}

void Input::endFrame() {
    keysPressed.reset();
    keysReleased.reset();
    mousePressed.reset();
    mouseReleased.reset();
    text.clear();
    mouseDelta = {};
    mouseScroll = {};

    std::erase_if(touches, [](const Touch& touch) { return !touch.isActive(); });
    for (Touch& touch : touches) {
        touch.phase = TouchPhase::Stationary;
        touch.previousPosition = touch.position;
    }
}

bool Input::isKeyDown(Key key) const noexcept {
    return keyIndex(key) < Controls::kKeyCount && keysDown.test(keyIndex(key));
}

bool Input::isKeyPressed(Key key) const noexcept {
    return keyIndex(key) < Controls::kKeyCount && keysPressed.test(keyIndex(key));
}

bool Input::isKeyReleased(Key key) const noexcept {
    return keyIndex(key) < Controls::kKeyCount && keysReleased.test(keyIndex(key));
}

bool Input::isMouseDown(MouseButton button) const noexcept {
    return mouseDown.test(buttonIndex(button));
}

bool Input::isMousePressed(MouseButton button) const noexcept {
    return mousePressed.test(buttonIndex(button));
}

bool Input::isMouseReleased(MouseButton button) const noexcept {
    return mouseReleased.test(buttonIndex(button));
}

const Touch* Input::findTouch(std::uint64_t id) const noexcept {
    const auto found = std::find_if(touches.begin(), touches.end(), [id](const Touch& candidate) { return candidate.id == id; });
    return found == touches.end() ? nullptr : &*found;
}

const GamepadState& Input::getGamepad(std::size_t index) const noexcept {
    return index < kMaxGamepads ? gamepads[index] : kDisconnectedGamepad;
}

bool Input::isGamepadDown(std::size_t index, GamepadButton button) const noexcept {
    return index < kMaxGamepads && gamepads[index].buttons[static_cast<std::size_t>(button)];
}

bool Input::isGamepadPressed(std::size_t index, GamepadButton button) const noexcept {
    const auto slot = static_cast<std::size_t>(button);
    return index < kMaxGamepads && gamepads[index].buttons[slot] && !previousGamepads[index].buttons[slot];
}

bool Input::isGamepadReleased(std::size_t index, GamepadButton button) const noexcept {
    const auto slot = static_cast<std::size_t>(button);
    return index < kMaxGamepads && !gamepads[index].buttons[slot] && previousGamepads[index].buttons[slot];
}

float Input::getGamepadAxis(std::size_t index, GamepadAxis axis) const noexcept {
    if (index >= kMaxGamepads) {
        return 0.0F;
    }
    return applyDeadzone(gamepads[index].axes[static_cast<std::size_t>(axis)], deadzone);
}

math::Vec2 Input::getGamepadStick(std::size_t index, bool rightStick) const noexcept {
    if (index >= kMaxGamepads) {
        return {};
    }

    const auto& axes = gamepads[index].axes;
    const auto axis = [&axes](GamepadAxis which) { return axes[static_cast<std::size_t>(which)]; };
    const math::Vec2 raw = rightStick ? math::Vec2{axis(GamepadAxis::RightX), axis(GamepadAxis::RightY)} : math::Vec2{axis(GamepadAxis::LeftX), axis(GamepadAxis::LeftY)};

    // Radial dead zone keeps diagonal movement smooth, unlike per-axis clipping.
    const float magnitude = raw.getLength();
    if (magnitude <= deadzone) {
        return {};
    }
    const float scaled = std::min(1.0F, (magnitude - deadzone) / (1.0F - deadzone));
    return raw / magnitude * scaled;
}

void Input::setGamepadDeadzone(float value) {
    if (!(value >= 0.0F && value < 1.0F)) {
        throw std::invalid_argument("The gamepad dead zone must be at least 0 and below 1.");
    }
    deadzone = value;
}

} // namespace haylen::input
