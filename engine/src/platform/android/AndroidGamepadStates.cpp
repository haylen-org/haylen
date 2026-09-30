#include "platform/android/AndroidGamepadStates.hpp"

#include <cstddef>

#include "haylen/input/GamepadAxis.hpp"

namespace haylen::platform {

void AndroidGamepadStates::press(std::int32_t device, input::GamepadButton button, bool down) {
    if (input::GamepadState* state = claim(device)) {
        state->buttons[static_cast<std::size_t>(button)] = down;
    }
}

void AndroidGamepadStates::move(std::int32_t device, const Motion& motion) {
    if (input::GamepadState* state = claim(device)) {
        apply(*state, motion);
    }
}

void AndroidGamepadStates::releaseAxes() {
    for (input::GamepadState& state : gamepads) {
        apply(state, {});
    }
}

void AndroidGamepadStates::remove(std::int32_t device) {
    for (std::size_t index = 0; index < devices.size(); ++index) {
        if (devices[index] == device) {
            devices[index].reset();
            gamepads[index] = {};
        }
    }
}

void AndroidGamepadStates::copyTo(std::span<input::GamepadState> states) const {
    for (std::size_t index = 0; index < states.size() && index < gamepads.size(); ++index) {
        states[index] = gamepads[index];
    }
}

void AndroidGamepadStates::apply(input::GamepadState& state, const Motion& motion) {
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = motion.leftX;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = motion.leftY;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightX)] = motion.rightX;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightY)] = motion.rightY;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftTrigger)] = motion.leftTrigger;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightTrigger)] = motion.rightTrigger;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadLeft)] = motion.hatX < -0.5F;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadRight)] = motion.hatX > 0.5F;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadUp)] = motion.hatY < -0.5F;
    state.buttons[static_cast<std::size_t>(input::GamepadButton::DpadDown)] = motion.hatY > 0.5F;
}

input::GamepadState* AndroidGamepadStates::claim(std::int32_t device) {
    for (std::size_t index = 0; index < devices.size(); ++index) {
        if (devices[index] == device) {
            return &gamepads[index];
        }
    }
    for (std::size_t index = 0; index < devices.size(); ++index) {
        if (!devices[index]) {
            devices[index] = device;
            gamepads[index] = {.connected = true, .name = "Controller"};
            return &gamepads[index];
        }
    }
    return nullptr;
}

} // namespace haylen::platform
