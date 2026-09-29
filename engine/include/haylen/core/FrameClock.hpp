#pragma once

#include <cstdint>

#include "haylen/core/ProcessMode.hpp"

namespace haylen::core {

// Converts variable frame durations into scaled frame time and fixed simulation steps, and holds the pause state that decides which process modes run. Pausing does not change the frame times, but no fixed steps accumulate while paused.
class FrameClock final {
  public:
    explicit FrameClock(double fixedStepSeconds = 1.0 / 60.0, double maxFrameSeconds = 0.25) noexcept;

    void advance(double frameSeconds) noexcept;
    [[nodiscard]] bool consumeFixedStep() noexcept;

    // Treats the next frame as taking no time, so a resume after the app was halted does not jump timers, tweens and physics forward.
    void skipNextDelta() noexcept {
        skipNext = true;
    }

    void setTimeScale(double value) noexcept;
    [[nodiscard]] double getTimeScale() const noexcept {
        return timeScale;
    }

    void setPaused(bool value) noexcept {
        paused = value;
    }
    [[nodiscard]] bool isPaused() const noexcept {
        return paused;
    }

    // Returns whether something with the mode runs in the current pause state. Inherit counts as Pausable, so callers resolve it against the parent first.
    [[nodiscard]] bool canProcess(ProcessMode mode) const noexcept {
        return canProcess(mode, paused);
    }
    [[nodiscard]] static bool canProcess(ProcessMode mode, bool gamePaused) noexcept;

    [[nodiscard]] double getDelta() const noexcept {
        return delta;
    }
    [[nodiscard]] double getUnscaledDelta() const noexcept {
        return unscaledDelta;
    }
    [[nodiscard]] double getFixedStep() const noexcept {
        return fixedStep;
    }
    [[nodiscard]] double getElapsed() const noexcept {
        return elapsed;
    }
    [[nodiscard]] std::uint64_t getFrameIndex() const noexcept {
        return frameIndex;
    }

    // Counts the fixed steps consumed since the frame began.
    [[nodiscard]] std::uint32_t getFixedStepCount() const noexcept {
        return fixedSteps;
    }

    // Returns how far the simulation is between the previous and the next fixed step, in [0, 1).
    [[nodiscard]] double getInterpolation() const noexcept {
        return accumulator / fixedStep;
    }

  private:
    double fixedStep;
    double maxFrameTime;
    double timeScale = 1.0;
    double delta = 0.0;
    double unscaledDelta = 0.0;
    double accumulator = 0.0;
    double elapsed = 0.0;
    std::uint64_t frameIndex = 0;
    std::uint32_t fixedSteps = 0;
    bool paused = false;
    bool skipNext = false;
};

} // namespace haylen::core
