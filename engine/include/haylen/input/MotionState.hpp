#pragma once

#include <array>
#include <bitset>

#include "haylen/input/Controls.hpp"

namespace haylen::input {

// The newest readings of the motion sensors of a gamepad, with the sensors that delivered them. The axes follow the face of a gamepad that lies flat in the hands, with x to the right, y away from the person and z out of the face. The acceleration counts gravity in meters per second squared, so a gamepad that lies still on a table reads 9.81 on z, the rotation rate turns counterclockwise around each axis in radians per second, and the attitude is the unit quaternion `{x, y, z, w}` that turns the axes of the gamepad into a frame whose z points up.
struct MotionState {
    std::bitset<Controls::kMotionSensorCount> sensors;
    std::array<float, 3> acceleration{};
    std::array<float, 3> rotationRate{};
    std::array<float, 4> attitude{0.0F, 0.0F, 0.0F, 1.0F};
};

} // namespace haylen::input
