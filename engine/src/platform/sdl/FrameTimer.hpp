#pragma once

#include <cstdint>

namespace haylen::platform::sdl {

// Measures the seconds between frames and smooths them, so the frames that the display paces evenly advance the app evenly although the system wakes the frame thread a little early or late. A frame that takes clearly longer or shorter than the ones before starts the smoothing again from its own duration.
class FrameTimer final {
  public:
    // Returns the smoothed seconds since the last call, for the frame that starts at the time in nanoseconds.
    [[nodiscard]] double next(std::uint64_t nanoseconds) noexcept;

  private:
    static constexpr double kMinSeconds = 0.000001;
    static constexpr double kMaxSeconds = 0.1;
    static constexpr double kResetSeconds = 0.004;
    static constexpr double kSmoothing = 0.025;

    std::uint64_t last = 0;
    double average = 1.0 / 60.0;
};

} // namespace haylen::platform::sdl
