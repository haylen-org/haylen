#include "platform/android/AndroidMotion.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "platform/MotionFrame.hpp"
#include "platform/android/JavaBridge.hpp"

namespace haylen::platform {

ASensorEventQueue* AndroidMotion::queue = nullptr;
input::MotionSensors::Sensors AndroidMotion::running;
input::MotionState AndroidMotion::latest;
std::atomic<int> AndroidMotion::quarterTurns = 0;

ASensorManager* AndroidMotion::getManager() {
    static ASensorManager* manager = ASensorManager_getInstanceForPackage(JavaBridge::getPackageName().c_str());
    return manager;
}

const ASensor* AndroidMotion::find(input::MotionSensor sensor) {
    return ASensorManager_getDefaultSensor(getManager(), kSensors[static_cast<std::size_t>(sensor)].second);
}

input::MotionSensors::Support AndroidMotion::getSupport() {
    input::MotionSensors::Support support;
    for (const auto& [sensor, type] : kSensors) {
        support.sensors.set(static_cast<std::size_t>(sensor), find(sensor) != nullptr);
    }
    if (support.sensors.none()) {
        support.reason = "This device has no motion sensors.";
    }
    return support;
}

void AndroidMotion::setQuarterTurns(int value) noexcept {
    quarterTurns.store(value);
}

void AndroidMotion::set(input::MotionSensors::Sensors sensors, float rate) {
    if (queue == nullptr && sensors.any()) {
        queue = ASensorManager_createEventQueue(getManager(), ALooper_forThread(), ALOOPER_POLL_CALLBACK, &receive, nullptr);
    }
    if (queue == nullptr) {
        return;
    }

    const auto period = static_cast<std::int32_t>(std::lround(1.0e6 / std::clamp(rate, input::MotionSensors::kMinRate, input::MotionSensors::kMaxRate)));
    for (const auto& [sensor, type] : kSensors) {
        const ASensor* found = find(sensor);
        const auto slot = static_cast<std::size_t>(sensor);
        if (found == nullptr) {
            continue;
        }
        if (running.test(slot)) {
            ASensorEventQueue_disableSensor(queue, found);
        }
        if (sensors.test(slot)) {
            ASensorEventQueue_registerSensor(queue, found, std::max(period, ASensor_getMinDelay(found)), 0);
        }
    }
    running = sensors;
    latest.sensors &= sensors;

    if (sensors.none()) {
        ASensorManager_destroyEventQueue(getManager(), queue);
        queue = nullptr;
    }
}

// Android measures the push that holds the device up in meters per second squared and the rotation in radians per second, as the engine does, and the rotation vector gives the quaternion of the attitude, whose w it leaves out on some devices.
int AndroidMotion::receive(int, int, void*) {
    ASensorEvent event;
    while (queue != nullptr && ASensorEventQueue_getEvents(queue, &event, 1) > 0) {
        if (event.type == ASENSOR_TYPE_ACCELEROMETER) {
            latest.acceleration = {event.acceleration.x, event.acceleration.y, event.acceleration.z};
            latest.sensors.set(static_cast<std::size_t>(input::MotionSensor::Accelerometer));
        } else if (event.type == ASENSOR_TYPE_GYROSCOPE) {
            latest.rotationRate = {event.vector.x, event.vector.y, event.vector.z};
            latest.sensors.set(static_cast<std::size_t>(input::MotionSensor::Gyroscope));
        } else if (event.type == ASENSOR_TYPE_GAME_ROTATION_VECTOR) {
            const float x = event.data[0];
            const float y = event.data[1];
            const float z = event.data[2];
            latest.attitude = {x, y, z, std::sqrt(std::max(0.0F, 1.0F - x * x - y * y - z * z))};
            latest.sensors.set(static_cast<std::size_t>(input::MotionSensor::Attitude));
        }
    }
    return 1;
}

void AndroidMotion::read(input::MotionState& state) {
    state = latest;
    MotionFrame::turn(state, quarterTurns.load());
}

} // namespace haylen::platform
