#include "platform/MotionFrame.hpp"

#include <cmath>
#include <numbers>

namespace haylen::platform {

std::array<float, 4> MotionFrame::multiply(const std::array<float, 4>& left, const std::array<float, 4>& right) noexcept {
    const auto [ax, ay, az, aw] = left;
    const auto [bx, by, bz, bw] = right;
    return {aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx, aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz};
}

std::array<float, 3> MotionFrame::rotate(const std::array<float, 3>& value, int quarterTurns) noexcept {
    switch (quarterTurns) {
    case 1:
        return {-value[1], value[0], value[2]};
    case 2:
        return {-value[0], -value[1], value[2]};
    case 3:
        return {value[1], -value[0], value[2]};
    default:
        return value;
    }
}

void MotionFrame::turn(input::MotionState& state, int quarterTurns) noexcept {
    const int turns = ((quarterTurns % 4) + 4) % 4;
    if (turns == 0) {
        return;
    }
    state.acceleration = rotate(state.acceleration, turns);
    state.rotationRate = rotate(state.rotationRate, turns);

    // The screen turned clockwise against the device, so its axes reach the world through that turn first.
    const float half = -static_cast<float>(turns) * std::numbers::pi_v<float> / 4.0F;
    state.attitude = multiply(state.attitude, {0.0F, 0.0F, std::sin(half), std::cos(half)});
}

std::array<float, 4> MotionFrame::fromAngles(double alpha, double beta, double gamma) noexcept {
    const auto half = [](double degrees) { return static_cast<float>(degrees * std::numbers::pi / 360.0); };
    const float z = half(alpha);
    const float x = half(beta);
    const float y = half(gamma);
    const std::array<float, 4> aroundZ{0.0F, 0.0F, std::sin(z), std::cos(z)};
    const std::array<float, 4> aroundX{std::sin(x), 0.0F, 0.0F, std::cos(x)};
    const std::array<float, 4> aroundY{0.0F, std::sin(y), 0.0F, std::cos(y)};
    return multiply(multiply(aroundZ, aroundX), aroundY);
}

// Browsers measure the push that holds the device up, as the engine does, and the rotation rate in degrees per second that they name alpha, beta and gamma for the x, y and z axes.
void MotionFrame::readBrowser(input::MotionState& state, const std::array<float, 9>& values, const std::bitset<input::Controls::kMotionSensorCount>& sensors, int quarterTurns) noexcept {
    constexpr float kRadiansPerDegree = std::numbers::pi_v<float> / 180.0F;
    state.sensors = sensors;
    if (sensors.test(static_cast<std::size_t>(input::MotionSensor::Accelerometer))) {
        state.acceleration = {values[0], values[1], values[2]};
    }
    if (sensors.test(static_cast<std::size_t>(input::MotionSensor::Gyroscope))) {
        state.rotationRate = {values[3] * kRadiansPerDegree, values[4] * kRadiansPerDegree, values[5] * kRadiansPerDegree};
    }
    if (sensors.test(static_cast<std::size_t>(input::MotionSensor::Attitude))) {
        state.attitude = fromAngles(values[6], values[7], values[8]);
    }
    turn(state, quarterTurns);
}

std::array<float, 3> MotionFrame::fromGamepadAxes(const std::array<float, 3>& value) noexcept {
    return {value[0], -value[2], value[1]};
}

} // namespace haylen::platform
