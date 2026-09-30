#pragma once

#include <game-activity/GameActivity.h>

#include <cstdint>
#include <mutex>
#include <span>

#include "haylen/input/GamepadButton.hpp"
#include "haylen/input/GamepadState.hpp"
#include "platform/android/AndroidGamepadStates.hpp"

namespace haylen::platform {

// Controllers read from the input events of GameActivity, which the frame thread receives while it polls them.
class AndroidGamepads final {
  public:
    // GameActivity copies only the position axes of motion events unless others are enabled, while controllers report sticks, triggers and hats on theirs.
    static void enableAxes();

    // Answers on the UI thread whether a key is a button of a controller, which the app takes from the views and the system, as the key callback of `sokol_app`.
    static bool takesKey(const void* keyEvent);

    // Takes an input event before `sokol_app` translates it, as its native event callback, and returns whether it was a controller event. It runs on the frame thread.
    static bool handleEvent(const void* source);

    static void poll(std::span<input::GamepadState> states);

    // Frees the slot of a controller that Android reports as removed.
    static void remove(std::int32_t device);

    // Centers the sticks and the triggers of every controller when the window of the app loses the focus, on the frame thread in order with the input events.
    static void releaseAxes();

  private:
    static std::mutex& mutex;
    static AndroidGamepadStates& controllers;

    [[nodiscard]] static bool isController(std::int32_t source) noexcept;
    [[nodiscard]] static bool handleKey(const GameActivityKeyEvent& event);
    static void handleMotion(const GameActivityMotionEvent& event);
    [[nodiscard]] static bool toButton(std::int32_t code, input::GamepadButton& button);
};

} // namespace haylen::platform
