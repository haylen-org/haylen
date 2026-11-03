#include "platform/FrameRateGate.hpp"

#include <algorithm>

namespace haylen::platform {

void FrameRateGate::setTarget(int framesPerSecond) noexcept {
    target = framesPerSecond;
    due.reset();
}

bool FrameRateGate::takeFrame(double now) noexcept {
    if (target <= 0) {
        return true;
    }

    const double interval = 1.0 / static_cast<double>(target);
    if (due && now < *due - interval * kEarlyShare) {
        return false;
    }

    // A frame on time keeps the beat of the ones before, and a late one starts it again.
    due = due && now < *due + interval ? *due + interval : now + interval;
    return true;
}

double FrameRateGate::getWaitSeconds(double now) const noexcept {
    if (target <= 0 || !due) {
        return 0.0;
    }
    return std::max(0.0, *due - kEarlyShare / static_cast<double>(target) - now);
}

} // namespace haylen::platform
