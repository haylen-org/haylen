#include "platform/sdl/FrameTimer.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::platform::sdl {

double FrameTimer::next(std::uint64_t nanoseconds) noexcept {
    if (last == 0) {
        last = nanoseconds;
        return average;
    }
    const double seconds = std::clamp(static_cast<double>(nanoseconds - last) / 1e9, kMinSeconds, kMaxSeconds);
    last = nanoseconds;

    if (std::abs(seconds - average) > kResetSeconds) {
        average = seconds;
        return average;
    }
    average += kSmoothing * (seconds - average);
    return average;
}

} // namespace haylen::platform::sdl
