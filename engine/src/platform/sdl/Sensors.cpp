#include "platform/sdl/Sensors.hpp"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_stdinc.h>

#include "platform/MotionFrame.hpp"

namespace haylen::platform::sdl {

Sensors::~Sensors() {
    set({});
    if (started) {
        SDL_QuitSubSystem(SDL_INIT_SENSOR);
    }
}

bool Sensors::start() {
    if (!started) {
        started = SDL_InitSubSystem(SDL_INIT_SENSOR);
    }
    return started;
}

input::MotionSensors::Support Sensors::getSupport() {
    input::MotionSensors::Support support;
    int count = 0;
    SDL_SensorID* sensors = start() ? SDL_GetSensors(&count) : nullptr;
    for (int index = 0; index < count; ++index) {
        for (const auto& [sensor, type] : kSensors) {
            if (SDL_GetSensorTypeForID(sensors[index]) == type) {
                support.sensors.set(static_cast<std::size_t>(sensor));
            }
        }
    }
    SDL_free(sensors);

    if (support.sensors.none()) {
#if defined(__APPLE__)
        support.reason = "Macs give apps no readings of motion sensors.";
#elif defined(_WIN32)
        support.reason = "This computer has no motion sensors.";
#else
        support.reason = "The Linux desktop gives apps no readings of motion sensors.";
#endif
    }
    return support;
}

void Sensors::set(input::MotionSensors::Sensors sensors) {
    for (std::size_t slot = 0; slot < kSensors.size(); ++slot) {
        const auto [sensor, type] = kSensors[slot];
        const bool wanted = sensors.test(static_cast<std::size_t>(sensor));
        if (!wanted && opened[slot] != nullptr) {
            SDL_CloseSensor(opened[slot]);
            opened[slot] = nullptr;
        }
        if (!wanted || opened[slot] != nullptr || !start()) {
            continue;
        }

        int count = 0;
        SDL_SensorID* found = SDL_GetSensors(&count);
        for (int index = 0; index < count && opened[slot] == nullptr; ++index) {
            if (SDL_GetSensorTypeForID(found[index]) == type) {
                opened[slot] = SDL_OpenSensor(found[index]);
            }
        }
        SDL_free(found);
    }
}

int Sensors::getQuarterTurns(SDL_DisplayOrientation orientation) noexcept {
    switch (orientation) {
    case SDL_ORIENTATION_LANDSCAPE:
        return 1;
    case SDL_ORIENTATION_PORTRAIT_FLIPPED:
        return 2;
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
        return 3;
    default:
        return 0;
    }
}

void Sensors::read(input::MotionState& state, SDL_Window* window) const {
    for (std::size_t slot = 0; slot < kSensors.size(); ++slot) {
        std::array<float, 3> values{};
        if (opened[slot] == nullptr || !SDL_GetSensorData(opened[slot], values.data(), static_cast<int>(values.size()))) {
            continue;
        }
        const auto [sensor, type] = kSensors[slot];
        state.sensors.set(static_cast<std::size_t>(sensor));
#if defined(_WIN32)
        // The Sensor API of Windows reports the pull of gravity, which reads -1 g on z for a tablet that lies face up, while the engine reads the push that holds the device up.
        if (type == SDL_SENSOR_ACCEL) {
            values = {-values[0], -values[1], -values[2]};
        }
#endif
        (type == SDL_SENSOR_ACCEL ? state.acceleration : state.rotationRate) = values;
    }

    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    MotionFrame::turn(state, getQuarterTurns(SDL_GetCurrentDisplayOrientation(display)) - getQuarterTurns(SDL_GetNaturalDisplayOrientation(display)));
}

} // namespace haylen::platform::sdl
