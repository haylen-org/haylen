#pragma once

#include <game-activity/GameActivity.h>

#include <cstdint>
#include <mutex>
#include <span>
#include <string>

#include "haylen/input/GamepadButton.hpp"
#include "haylen/input/GamepadState.hpp"
#include "platform/android/AndroidGamepadStates.hpp"

namespace haylen::platform {

// Controllers read from the input events of GameActivity, which the frame thread receives while it polls them.
class AndroidGamepads final {
  public:
    // GameActivity copies only the position axes of motion events unless others are enabled, while controllers report sticks, triggers and hats on theirs.
    static void enableAxes();

    // Answers on the UI thread whether a key is a button of a controller, which the app takes from the views and the system.
    [[nodiscard]] static bool takesKey(const GameActivityKeyEvent& event);

    // Take a key or a motion event before the backend translates it and return whether it came from a controller. They run on the frame thread.
    [[nodiscard]] static bool handleKey(const GameActivityKeyEvent& event);
    [[nodiscard]] static bool handleMotion(const GameActivityMotionEvent& event);

    static void poll(std::span<input::GamepadState> states);

    // Gives a controller that Android adds its slot, its name and the labels of the vendor id of its maker, and frees the slot of one that Android removes.
    static void add(std::int32_t device, std::string name, std::uint16_t vendor);
    static void remove(std::int32_t device);

    // Centers the sticks and the triggers of every controller when the window of the app loses the focus, on the frame thread in order with the input events.
    static void releaseAxes();

    // Runs the vibrators of the controller in a slot, which Android gives controllers that have them.
    static void rumble(std::size_t slot, float low, float high, float seconds);

  private:
    static std::mutex& mutex;
    static AndroidGamepadStates& controllers;

    [[nodiscard]] static bool isController(std::int32_t source) noexcept;
    static void readMotion(const GameActivityMotionEvent& event);
    [[nodiscard]] static bool toButton(std::int32_t code, input::GamepadButton& button);
};

} // namespace haylen::platform
