#pragma once

#include <cstdint>

#include "haylen/math/Vec2.hpp"

namespace haylen::input {

// A finger on a touchpad of a gamepad, with the touchpad it touches, its position from 0 to 1 across the touchpad from the top left corner and its pressure from 0 to 1.
struct TouchpadFinger {
    bool down = false;
    std::uint8_t touchpad = 0;
    math::Vec2 position{};
    float pressure = 0.0F;
};

} // namespace haylen::input
