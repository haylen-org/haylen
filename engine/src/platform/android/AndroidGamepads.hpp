#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <span>

#include "haylen/input/GamepadButton.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/input/Input.hpp"

namespace haylen::platform {

// Controllers read from the raw input events of Android, which arrive on the input thread while the frame thread polls them.
class AndroidGamepads final {
  public:
    // Takes a raw input event before sokol_app sees it, as its native event callback, and returns whether it was a controller event.
    static bool handleEvent(const void* source);

    static void poll(std::span<input::GamepadState> states);

    // Frees the slot of a controller that Android reports as removed.
    static void remove(std::int32_t device);

  private:
    static std::mutex& mutex;
    static std::array<std::int32_t, input::Input::kMaxGamepads> devices;
    static std::array<input::GamepadState, input::Input::kMaxGamepads>& gamepads;

    [[nodiscard]] static int findSlot(std::int32_t device);
    [[nodiscard]] static bool toButton(std::int32_t code, input::GamepadButton& button);
};

} // namespace haylen::platform
