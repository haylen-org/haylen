#include "platform/FrameCallbackGate.hpp"

namespace haylen::platform {

bool FrameCallbackGate::takePresent(double now) noexcept {
    if (suspended || waiting) {
        return false;
    }

    waiting = true;
    askedAt = now;
    return true;
}

void FrameCallbackGate::receiveCallback() noexcept {
    waiting = false;
}

bool FrameCallbackGate::tick(double now) const noexcept {
    return !waiting || now - askedAt >= kHeldSeconds;
}

} // namespace haylen::platform
