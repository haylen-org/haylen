#pragma once

#include <cstdint>

namespace haylen::input {

// A gamepad button named by its position, so `South` is A on an Xbox pad and Cross on a PlayStation pad.
enum class GamepadButton : std::uint8_t {
    South,
    East,
    West,
    North,
    LeftShoulder,
    RightShoulder,
    Back,
    Start,
    Guide,
    LeftStick,
    RightStick,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
};

} // namespace haylen::input
