#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <span>
#include <string>

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

    // Plays a rumble on the event device of the controller in the slot, which the kernel offers for controllers with force feedback, or stops it. The event device opens at the first rumble, and a controller whose event device the user may not write does nothing.
    static void rumble(std::size_t slot, float low, float high, float seconds);

  private:
    struct Joystick {
        int descriptor = -1;
        int feedback = -1;
        int effect = -1;
        input::GamepadState state;
    };

    [[nodiscard]] static std::string getDeviceFolder(std::size_t slot);
    [[nodiscard]] static input::GamepadLabels readLabels(std::size_t slot);
    [[nodiscard]] static int openFeedback(std::size_t slot);
    static void close(Joystick& joystick) noexcept;
    static bool play(int descriptor, int effect, bool started);

    static std::array<Joystick, input::Input::kMaxGamepads>& joysticks;
    static std::chrono::steady_clock::time_point nextScan;

    static void apply(input::GamepadState& state, const js_event& event);
};

} // namespace haylen::platform
