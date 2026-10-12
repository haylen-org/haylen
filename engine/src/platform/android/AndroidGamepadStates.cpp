#include "platform/android/AndroidGamepadStates.hpp"

#include <cstddef>
#include <utility>

#include "haylen/input/GamepadAxis.hpp"

namespace haylen::platform {

void AndroidGamepadStates::press(std::int32_t device, input::GamepadButton button, bool down) {
    if (const std::optional<std::size_t> slot = claim(device)) {
        gamepads[*slot].buttons[static_cast<std::size_t>(button)] = down;
    }
}

void AndroidGamepadStates::move(std::int32_t device, const Motion& motion) {
    if (const std::optional<std::size_t> slot = claim(device)) {
        apply(*slot, motion);
    }
}

void AndroidGamepadStates::releaseAxes() {
    for (std::size_t slot = 0; slot < gamepads.size(); ++slot) {
        apply(slot, {});
    }
}

void AndroidGamepadStates::add(std::int32_t device, std::string name) {
    if (const std::optional<std::size_t> slot = claim(device)) {
        gamepads[*slot].name = std::move(name);
    }
}

void AndroidGamepadStates::remove(std::int32_t device) {
    for (std::size_t index = 0; index < devices.size(); ++index) {
        if (devices[index] == device) {
            devices[index].reset();
            gamepads[index] = {};
            hats[index] = {};
        }
    }
}

// The directional pad is down while either its keys or the hat hold a direction.
void AndroidGamepadStates::copyTo(std::span<input::GamepadState> states) const {
    for (std::size_t index = 0; index < states.size() && index < gamepads.size(); ++index) {
        states[index] = gamepads[index];
        for (std::size_t direction = 0; direction < kDirections.size(); ++direction) {
            states[index].buttons[static_cast<std::size_t>(kDirections[direction])] = states[index].buttons[static_cast<std::size_t>(kDirections[direction])] || hats[index].test(direction);
        }
    }
}

// The hat moves the directional pad apart from its keys, so a stick that moves never releases a direction that a key of the pad holds.
void AndroidGamepadStates::apply(std::size_t slot, const Motion& motion) {
    input::GamepadState& state = gamepads[slot];
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftX)] = motion.leftX;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftY)] = motion.leftY;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightX)] = motion.rightX;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightY)] = motion.rightY;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::LeftTrigger)] = motion.leftTrigger;
    state.axes[static_cast<std::size_t>(input::GamepadAxis::RightTrigger)] = motion.rightTrigger;
    std::bitset<4>& hat = hats[slot];
    hat.set(0, motion.hatX < -0.5F);
    hat.set(1, motion.hatX > 0.5F);
    hat.set(2, motion.hatY < -0.5F);
    hat.set(3, motion.hatY > 0.5F);
}

std::optional<std::size_t> AndroidGamepadStates::claim(std::int32_t device) {
    for (std::size_t index = 0; index < devices.size(); ++index) {
        if (devices[index] == device) {
            return index;
        }
    }
    for (std::size_t index = 0; index < devices.size(); ++index) {
        if (!devices[index]) {
            devices[index] = device;
            gamepads[index] = {.connected = true, .name = "Controller"};
            hats[index] = {};
            return index;
        }
    }
    return std::nullopt;
}

} // namespace haylen::platform
