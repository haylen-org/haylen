#pragma once

#include <android/looper.h>
#include <android/sensor.h>

#include <array>
#include <atomic>

#include "haylen/input/MotionSensors.hpp"
#include "haylen/input/MotionState.hpp"

namespace haylen::platform {

// The motion sensors of the device through the sensor manager of the native runtime, whose events reach the looper of the frame thread while sensors run: the accelerometer, the gyroscope and the game rotation vector, which fuses them into an attitude whose z points up. Android asks the person for none of them, and reads them at most 200 times per second, since faster readings need the permission `HIGH_SAMPLING_RATE_SENSORS`. Every method runs on the frame thread, except `setQuarterTurns`.
class AndroidMotion final {
  public:
    [[nodiscard]] static input::MotionSensors::Support getSupport();
    static void set(input::MotionSensors::Sensors sensors, float rate);
    static void read(input::MotionState& state);

    // The quarter turns counterclockwise that the display of the activity turned from its natural orientation, which the activity reports from the main thread whenever the display changes.
    static void setQuarterTurns(int value) noexcept;

  private:
    static constexpr std::array<std::pair<input::MotionSensor, int>, 3> kSensors{{
        {input::MotionSensor::Accelerometer, ASENSOR_TYPE_ACCELEROMETER},
        {input::MotionSensor::Gyroscope, ASENSOR_TYPE_GYROSCOPE},
        {input::MotionSensor::Attitude, ASENSOR_TYPE_GAME_ROTATION_VECTOR},
    }};

    [[nodiscard]] static ASensorManager* getManager();
    [[nodiscard]] static const ASensor* find(input::MotionSensor sensor);

    // Takes the events that wait in the queue into the newest readings, from the looper of the frame thread.
    static int receive(int descriptor, int events, void* data);

    static ASensorEventQueue* queue;
    static input::MotionSensors::Sensors running;
    static input::MotionState latest;
    static std::atomic<int> quarterTurns;
};

} // namespace haylen::platform
