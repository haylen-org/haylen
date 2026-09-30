#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "haylen/input/GamepadButton.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/input/Input.hpp"

namespace haylen::platform {

// The controllers that the input events of Android report, each in the gamepad slot that its device takes with its first event and keeps until Android removes the device. It compiles on every platform, so the tests of every host check it.
class AndroidGamepadStates final {
  public:
    // The sticks and the triggers of one motion event, and the hat that many controllers report the directional pad with.
    struct Motion {
        float leftX = 0.0F;
        float leftY = 0.0F;
        float rightX = 0.0F;
        float rightY = 0.0F;
        float leftTrigger = 0.0F;
        float rightTrigger = 0.0F;
        float hatX = 0.0F;
        float hatY = 0.0F;
    };

    // A controller that finds every slot taken changes nothing until a slot frees up.
    void press(std::int32_t device, input::GamepadButton button, bool down);
    void move(std::int32_t device, const Motion& motion);

    // Centers the sticks, the triggers and the hat of every controller, whose next motion event brings their position back. Android sends motion events only to the window with the focus, so a stick released while another window has it would keep its last value otherwise, while the releases of buttons still arrive.
    void releaseAxes();

    // Frees the slot of a controller that Android removed.
    void remove(std::int32_t device);

    void copyTo(std::span<input::GamepadState> states) const;

  private:
    static void apply(input::GamepadState& state, const Motion& motion);

    // Returns the state of the controller, which takes the first free slot with its first event, or null while every slot is taken.
    [[nodiscard]] input::GamepadState* claim(std::int32_t device);

    std::array<std::optional<std::int32_t>, input::Input::kMaxGamepads> devices{};
    std::array<input::GamepadState, input::Input::kMaxGamepads> gamepads{};
};

} // namespace haylen::platform
