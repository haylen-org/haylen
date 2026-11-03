#pragma once

#include <array>
#include <utility>

#include <SDL3/SDL_sensor.h>
#include <SDL3/SDL_video.h>

#include "haylen/input/MotionSensors.hpp"
#include "haylen/input/MotionState.hpp"

namespace haylen::platform::sdl {

// The motion sensors of a computer that the sensor subsystem of SDL reads, such as the accelerometer and the gyroscope that Windows tablets report through the Sensor API of Windows. Macs and the Linux desktop give apps no readings of such sensors, so SDL finds none there.
class Sensors final {
  public:
    Sensors() = default;
    Sensors(const Sensors&) = delete;
    Sensors& operator=(const Sensors&) = delete;
    ~Sensors();

    [[nodiscard]] input::MotionSensors::Support getSupport();

    // Opens the sensors in the set and closes the others. SDL reads each one at the rate of its hardware.
    void set(input::MotionSensors::Sensors sensors);

    // Reads the sensors that are open in the axes of the screen that shows the window.
    void read(input::MotionState& state, SDL_Window* window) const;

  private:
    static constexpr std::array<std::pair<input::MotionSensor, SDL_SensorType>, 2> kSensors{{
        {input::MotionSensor::Accelerometer, SDL_SENSOR_ACCEL},
        {input::MotionSensor::Gyroscope, SDL_SENSOR_GYRO},
    }};

    // The quarter turns counterclockwise from portrait that SDL reports a display in.
    [[nodiscard]] static int getQuarterTurns(SDL_DisplayOrientation orientation) noexcept;

    // Starts the sensor subsystem of SDL the first time the app looks for sensors, so an app that never asks never starts it.
    [[nodiscard]] bool start();

    std::array<SDL_Sensor*, kSensors.size()> opened{};
    bool started = false;
};

} // namespace haylen::platform::sdl
