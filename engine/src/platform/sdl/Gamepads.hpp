#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include "haylen/input/GamepadMotion.hpp"
#include "haylen/input/GamepadState.hpp"
#include "platform/GamepadSlots.hpp"

namespace haylen::platform::sdl {

// The controllers that the gamepad API of SDL reads, each one in the slot it took when it connected. SDL lays every controller it knows out as a gamepad with its face buttons by their place, from XInput, Windows.Gaming.Input, HID and raw input on Windows, from evdev and HID on Linux, from GameController and HID on macOS and from the Gamepad API of the browser on the web.
class Gamepads final {
  public:
    Gamepads() = default;
    Gamepads(const Gamepads&) = delete;
    Gamepads& operator=(const Gamepads&) = delete;
    ~Gamepads();

    // Opens a controller that SDL connected and closes one that SDL lost. SDL reports every controller that is connected when its gamepad subsystem starts this way too.
    void handle(const SDL_Event& event);

    void poll(std::span<input::GamepadState> states);

    // Runs the motors of the controller in the slot for the seconds, or stops them at zero intensities.
    void rumble(std::size_t slot, float low, float high, float seconds) const;

    // Runs the accelerometer and the gyroscope in the set on every controller that has them, and on those that connect later, or stops them.
    void setMotion(input::GamepadMotion::Sensors sensors);

    [[nodiscard]] static input::GamepadLabels getLabels(SDL_GamepadButtonLabel south) noexcept;

    // The position of a stick from -1 to 1, with down and right positive, as the engine reads them.
    [[nodiscard]] static float readStick(Sint16 value) noexcept;

    [[nodiscard]] static float readTrigger(Sint16 value) noexcept;

  private:
    static constexpr std::array<std::pair<SDL_GamepadButton, input::GamepadButton>, 15> kButtons{{
        {SDL_GAMEPAD_BUTTON_SOUTH, input::GamepadButton::South},
        {SDL_GAMEPAD_BUTTON_EAST, input::GamepadButton::East},
        {SDL_GAMEPAD_BUTTON_WEST, input::GamepadButton::West},
        {SDL_GAMEPAD_BUTTON_NORTH, input::GamepadButton::North},
        {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, input::GamepadButton::LeftShoulder},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, input::GamepadButton::RightShoulder},
        {SDL_GAMEPAD_BUTTON_BACK, input::GamepadButton::Back},
        {SDL_GAMEPAD_BUTTON_START, input::GamepadButton::Start},
        {SDL_GAMEPAD_BUTTON_GUIDE, input::GamepadButton::Guide},
        {SDL_GAMEPAD_BUTTON_LEFT_STICK, input::GamepadButton::LeftStick},
        {SDL_GAMEPAD_BUTTON_RIGHT_STICK, input::GamepadButton::RightStick},
        {SDL_GAMEPAD_BUTTON_DPAD_UP, input::GamepadButton::DpadUp},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, input::GamepadButton::DpadDown},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, input::GamepadButton::DpadLeft},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, input::GamepadButton::DpadRight},
    }};

    static constexpr std::array<std::pair<input::MotionSensor, SDL_SensorType>, 2> kSensors{{
        {input::MotionSensor::Accelerometer, SDL_SENSOR_ACCEL},
        {input::MotionSensor::Gyroscope, SDL_SENSOR_GYRO},
    }};

    [[nodiscard]] SDL_Gamepad* find(const void* identity) const noexcept;
    void applyMotion(SDL_Gamepad* pad) const;
    static void read(SDL_Gamepad* pad, input::GamepadState& state);
    static void readMotion(SDL_Gamepad* pad, input::GamepadState& state);

    // Reads a vector of SDL, whose sensors point y out of the face of a controller and z toward the person, into the axes of a gamepad lying flat.
    [[nodiscard]] static std::array<float, 3> toFlatAxes(const std::array<float, 3>& value) noexcept;

    static void readTouchpads(SDL_Gamepad* pad, input::GamepadState& state);

    // The open controllers in the order they connected, and the list of their identities, which keeps its memory from frame to frame.
    std::vector<SDL_Gamepad*> opened;
    std::vector<const void*> connected;
    GamepadSlots slots;
    input::GamepadMotion::Sensors motion;
};

} // namespace haylen::platform::sdl
