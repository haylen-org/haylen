#pragma once

#include <cstdint>

namespace haylen::input {

enum class GamepadAxis : std::uint8_t {
    LeftX,
    LeftY,
    RightX,
    RightY,
    LeftTrigger,
    RightTrigger,
};

} // namespace haylen::input
