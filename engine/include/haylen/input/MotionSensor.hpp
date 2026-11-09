#pragma once

#include <cstdint>

namespace haylen::input {

// A motion sensor of a gamepad: the accelerometer, the gyroscope, or the attitude that the system fuses from them.
enum class MotionSensor : std::uint8_t {
    Accelerometer,
    Gyroscope,
    Attitude,
};

} // namespace haylen::input
