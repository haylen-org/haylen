#pragma once

#include <array>
#include <chrono>
#include <span>

#include "haylen/input/GamepadState.hpp"
#include "haylen/input/Input.hpp"

struct js_event;

namespace haylen::platform {

// Controllers read through the Linux joystick interface, one device node per slot. Empty slots are opened again every second, so controllers can be plugged in at any time.
class LinuxGamepads final {
  public:
    // Opens the device nodes of the empty slots.
    static void scan();

    // Closes every device node.
    static void release() noexcept;

    static void poll(std::span<input::GamepadState> states);

  private:
    struct Joystick {
        int descriptor = -1;
        input::GamepadState state;
    };

    static std::array<Joystick, input::Input::kMaxGamepads> joysticks;
    static std::chrono::steady_clock::time_point nextScan;

    static void apply(input::GamepadState& state, const js_event& event);
};

} // namespace haylen::platform
