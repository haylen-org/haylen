#include "platform/android/AndroidGamepads.hpp"

#include <android/input.h>

#include <algorithm>
#include <initializer_list>

#include "sokol_app.h"

namespace haylen::platform {

std::mutex& AndroidGamepads::mutex = *new std::mutex();
std::array<std::int32_t, input::Input::kMaxGamepads> AndroidGamepads::devices{-1, -1, -1, -1};
std::array<input::GamepadState, input::Input::kMaxGamepads>& AndroidGamepads::gamepads = *new std::array<input::GamepadState, input::Input::kMaxGamepads>();

void AndroidGamepads::enableAxes() {
    for (const std::int32_t axis : {AMOTION_EVENT_AXIS_Z, AMOTION_EVENT_AXIS_RZ, AMOTION_EVENT_AXIS_LTRIGGER, AMOTION_EVENT_AXIS_RTRIGGER, AMOTION_EVENT_AXIS_BRAKE, AMOTION_EVENT_AXIS_GAS, AMOTION_EVENT_AXIS_HAT_X, AMOTION_EVENT_AXIS_HAT_Y}) {
        GameActivityPointerAxes_enableAxis(axis);
    }
}

bool AndroidGamepads::takesKey(const void* keyEvent) {
    const auto& event = *static_cast<const GameActivityKeyEvent*>(keyEvent);
    input::GamepadButton button{};
    return isController(event.source) && toButton(event.keyCode, button);
}

bool AndroidGamepads::handleEvent(const void* source) {
    const auto& event = *static_cast<const sapp_android_native_event*>(source);
    if (event.type == SAPP_ANDROID_NATIVE_EVENT_KEY) {
        return handleKey(*static_cast<const GameActivityKeyEvent*>(event.event));
    }
    const auto& motion = *static_cast<const GameActivityMotionEvent*>(event.event);
    if (!isController(motion.source)) {
        return false;
    }
    handleMotion(motion);
    return true;
}

void AndroidGamepads::poll(std::span<input::GamepadState> states) {
    const std::scoped_lock lock(mutex);
    for (std::size_t index = 0; index < states.size() && index < gamepads.size(); ++index) {
        states[index] = gamepads[index];
    }
}

void AndroidGamepads::remove(std::int32_t device) {
    const std::scoped_lock lock(mutex);
    for (std::size_t index = 0; index < devices.size(); ++index) {
        if (devices[index] == device) {
            devices[index] = -1;
            gamepads[index] = {};
        }
    }
}

bool AndroidGamepads::isController(std::int32_t source) noexcept {
    const auto origin = static_cast<std::uint32_t>(source);
    return (origin & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD || (origin & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK;
}

bool AndroidGamepads::handleKey(const GameActivityKeyEvent& event) {
    input::GamepadButton button{};
    if (!isController(event.source) || !toButton(event.keyCode, button)) {
        return false;
    }

    const std::scoped_lock lock(mutex);
    const int slot = findSlot(event.deviceId);
    if (slot >= 0) {
        gamepads[static_cast<std::size_t>(slot)].buttons[static_cast<std::size_t>(button)] = event.action != AKEY_EVENT_ACTION_UP;
    }
    return true;
}

void AndroidGamepads::handleMotion(const GameActivityMotionEvent& event) {
    const GameActivityPointerAxes& axes = event.pointers[0];
    const auto value = [&axes](std::int32_t axis) { return GameActivityPointerAxes_getAxisValue(&axes, axis); };
    const std::scoped_lock lock(mutex);
    const int slot = findSlot(event.deviceId);
    if (slot < 0) {
        return;
    }

    input::GamepadState& state = gamepads[static_cast<std::size_t>(slot)];
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = value(AMOTION_EVENT_AXIS_X);
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = value(AMOTION_EVENT_AXIS_Y);
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightX)] = value(AMOTION_EVENT_AXIS_Z);
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightY)] = value(AMOTION_EVENT_AXIS_RZ);
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftTrigger)] = std::max(value(AMOTION_EVENT_AXIS_LTRIGGER), value(AMOTION_EVENT_AXIS_BRAKE));
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightTrigger)] = std::max(value(AMOTION_EVENT_AXIS_RTRIGGER), value(AMOTION_EVENT_AXIS_GAS));

    // Many controllers report the directional pad as a hat axis instead of keys.
    const float hatX = value(AMOTION_EVENT_AXIS_HAT_X);
    const float hatY = value(AMOTION_EVENT_AXIS_HAT_Y);
    state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadLeft)] = hatX < -0.5F;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadRight)] = hatX > 0.5F;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadUp)] = hatY < -0.5F;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadDown)] = hatY > 0.5F;
}

int AndroidGamepads::findSlot(std::int32_t device) {
    for (std::size_t index = 0; index < devices.size(); ++index) {
        if (devices[index] == device) {
            return static_cast<int>(index);
        }
    }
    for (std::size_t index = 0; index < devices.size(); ++index) {
        if (devices[index] < 0) {
            devices[index] = device;
            gamepads[index] = {.connected = true, .name = "Controller"};
            return static_cast<int>(index);
        }
    }
    return -1;
}

bool AndroidGamepads::toButton(std::int32_t code, input::GamepadButton& button) {
    switch (code) {
    case AKEYCODE_BUTTON_A:
        button = input::GamepadButton::South;
        return true;
    case AKEYCODE_BUTTON_B:
        button = input::GamepadButton::East;
        return true;
    case AKEYCODE_BUTTON_X:
        button = input::GamepadButton::West;
        return true;
    case AKEYCODE_BUTTON_Y:
        button = input::GamepadButton::North;
        return true;
    case AKEYCODE_BUTTON_L1:
        button = input::GamepadButton::LeftShoulder;
        return true;
    case AKEYCODE_BUTTON_R1:
        button = input::GamepadButton::RightShoulder;
        return true;
    case AKEYCODE_BUTTON_SELECT:
        button = input::GamepadButton::Back;
        return true;
    case AKEYCODE_BUTTON_START:
        button = input::GamepadButton::Start;
        return true;
    case AKEYCODE_BUTTON_MODE:
        button = input::GamepadButton::Guide;
        return true;
    case AKEYCODE_BUTTON_THUMBL:
        button = input::GamepadButton::LeftStick;
        return true;
    case AKEYCODE_BUTTON_THUMBR:
        button = input::GamepadButton::RightStick;
        return true;
    case AKEYCODE_DPAD_UP:
        button = input::GamepadButton::DpadUp;
        return true;
    case AKEYCODE_DPAD_DOWN:
        button = input::GamepadButton::DpadDown;
        return true;
    case AKEYCODE_DPAD_LEFT:
        button = input::GamepadButton::DpadLeft;
        return true;
    case AKEYCODE_DPAD_RIGHT:
        button = input::GamepadButton::DpadRight;
        return true;
    default:
        return false;
    }
}

} // namespace haylen::platform
