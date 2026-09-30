#pragma once

#include <game-activity/GameActivity.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <span>

#include "haylen/input/GamepadButton.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/input/Input.hpp"

namespace haylen::platform {

// Controllers read from the input events of GameActivity, which the frame thread receives while it polls them.
class AndroidGamepads final {
  public:
    // GameActivity copies only the position axes of motion events unless others are enabled, while controllers report sticks, triggers and hats on theirs.
    static void enableAxes();

    // Answers on the UI thread whether a key is a button of a controller, which the app takes from the views and the system, as the key callback of sokol_app.
    static bool takesKey(const void* keyEvent);

    // Takes an input event before sokol_app translates it, as its native event callback, and returns whether it was a controller event. It runs on the frame thread.
    static bool handleEvent(const void* source);

    static void poll(std::span<input::GamepadState> states);

    // Frees the slot of a controller that Android reports as removed.
    static void remove(std::int32_t device);

  private:
    static std::mutex& mutex;
    static std::array<std::int32_t, input::Input::kMaxGamepads> devices;
    static std::array<input::GamepadState, input::Input::kMaxGamepads>& gamepads;

    [[nodiscard]] static bool isController(std::int32_t source) noexcept;
    [[nodiscard]] static bool handleKey(const GameActivityKeyEvent& event);
    static void handleMotion(const GameActivityMotionEvent& event);
    [[nodiscard]] static int findSlot(std::int32_t device);
    [[nodiscard]] static bool toButton(std::int32_t code, input::GamepadButton& button);
};

} // namespace haylen::platform
