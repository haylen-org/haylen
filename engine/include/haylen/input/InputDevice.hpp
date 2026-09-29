#pragma once

#include <cstdint>

namespace haylen::input {

// The kind of device the player used last, which lets an app show matching prompts.
enum class InputDevice : std::uint8_t {
    KeyboardMouse,
    Touch,
    Gamepad,
};

} // namespace haylen::input
