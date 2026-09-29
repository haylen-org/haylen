#include "haylen/core/FrameClock.hpp"

#include <algorithm>

namespace haylen::core {

FrameClock::FrameClock(double fixedStepSeconds, double maxFrameSeconds) noexcept : fixedStep(fixedStepSeconds), maxFrameTime(maxFrameSeconds) {}

void FrameClock::advance(double frameSeconds) noexcept {
    unscaledDelta = skipNext ? 0.0 : std::clamp(frameSeconds, 0.0, maxFrameTime);
    skipNext = false;
    delta = unscaledDelta * timeScale;
    if (!paused) {
        accumulator += delta;
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

void FrameClock::setTimeScale(double value) noexcept {
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
