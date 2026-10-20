#include "haylen/core/FrameClock.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace haylen::core {

FrameClock::FrameClock(double fixedStepSeconds, double maxFrameSeconds) noexcept : fixedStep(fixedStepSeconds), maxFrameTime(maxFrameSeconds) {}

void FrameClock::advance(double frameSeconds) noexcept {
    unscaledDelta = skipNext ? 0.0 : std::clamp(frameSeconds, 0.0, maxFrameTime);
    skipNext = false;
    delta = unscaledDelta * timeScale;
    if (!paused) {
        accumulator = std::min(accumulator + delta, fixedStep * kMaxFixedSteps);
    }
    elapsed += delta;
    ++frameIndex;
    fixedSteps = 0;
}

bool FrameClock::consumeFixedStep() noexcept {
    if (paused || accumulator < fixedStep) {
        return false;
    }
    accumulator -= fixedStep;
    ++fixedSteps;
    return true;
}

// An infinite scale would turn every scaled time into infinity or NaN.
void FrameClock::setTimeScale(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("The time scale must be a finite number.");
    }
    timeScale = std::max(0.0, value);
}

bool FrameClock::canProcess(ProcessMode mode, bool gamePaused) noexcept {
    switch (mode) {
    case ProcessMode::Inherit:
    case ProcessMode::Pausable:
        return !gamePaused;
    case ProcessMode::WhenPaused:
        return gamePaused;
    case ProcessMode::Always:
        return true;
    case ProcessMode::Disabled:
        return false;
    }
    return false;
}

} // namespace haylen::core
