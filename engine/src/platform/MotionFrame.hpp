#pragma once

#include <array>

#include "haylen/input/MotionState.hpp"

namespace haylen::platform {

// Reads the axes and the units that the sensors of each system report into those of `input::MotionState`.
class MotionFrame final {
  public:
    // Turns readings along the axes of the device in its natural orientation into the axes of the screen, for a device turned counterclockwise by the quarter turns while the screen keeps the app upright.
    static void turn(input::MotionState& state, int quarterTurns) noexcept;

    // The attitude of the angles in degrees that browsers report, which turn the device around z by alpha, then around its new x by beta, then around its new y by gamma.
    [[nodiscard]] static std::array<float, 4> fromAngles(double alpha, double beta, double gamma) noexcept;

    // Reads what browsers report into the state: the acceleration with gravity in meters per second squared, the rotation rate in degrees per second around z, x and y as alpha, beta and gamma, and the angles of the attitude in degrees, for the sensors that delivered them, turned by the quarter turns of the screen.
    static void readBrowser(input::MotionState& state, const std::array<float, 9>& values, const std::bitset<input::Controls::kMotionSensorCount>& sensors, int quarterTurns) noexcept;

    // Reads a vector of a gamepad whose sensors point y out of its face and z toward the person, as those of SDL do, into the axes of a gamepad lying flat.
    [[nodiscard]] static std::array<float, 3> fromGamepadAxes(const std::array<float, 3>& value) noexcept;

  private:
    [[nodiscard]] static std::array<float, 4> multiply(const std::array<float, 4>& left, const std::array<float, 4>& right) noexcept;
    [[nodiscard]] static std::array<float, 3> rotate(const std::array<float, 3>& value, int quarterTurns) noexcept;
};

} // namespace haylen::platform
