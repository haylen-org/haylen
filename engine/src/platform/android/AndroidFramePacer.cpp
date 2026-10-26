#include "platform/android/AndroidFramePacer.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::platform {

bool AndroidFramePacer::takeTick(double now) noexcept {
    if (!isTicking() || now < nextTick) {
        return false;
    }
    nextTick = now + kTickSeconds;
    return true;
}

int AndroidFramePacer::getWaitMilliseconds(double now) const noexcept {
    if (!isTicking()) {
        return -1;
    }
    return std::max(0, static_cast<int>(std::ceil((nextTick - now) * 1000.0)));
}

double AndroidFramePacer::takeElapsed(double now) noexcept {
    const double elapsed = last ? std::max(0.0, now - *last) : 0.0;
    last = now;
    return elapsed;
}

} // namespace haylen::platform
