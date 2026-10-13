#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>

namespace haylen::platform {

// Runs the motors of XInput controllers, which turn until something stops them, so every rumble ends once its time passes at the next poll of the gamepads.
class WindowsRumble final {
  public:
    static void start(std::size_t index, float low, float high, float seconds);
    static void update();

  private:
    static constexpr std::size_t kControllers = 4;

    static std::array<std::optional<std::chrono::steady_clock::time_point>, kControllers> ends;

    static void set(std::size_t index, float low, float high);
};

} // namespace haylen::platform
