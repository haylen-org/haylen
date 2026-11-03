#pragma once

#include <optional>

namespace haylen::platform {

// Decides which refreshes of the display play a frame of an app that targets fewer frames per second than the display shows, such as 30 frames on a display of 60 or 120 hertz, so every frame comes a whole number of refreshes after the one before and the frames average the target. A refresh plays when it comes no more than a quarter of a frame early, and a frame that comes late starts the count again from itself. A target of 0 plays every refresh. Times are seconds of a monotonic clock, and it compiles on every platform, so the tests of every host check it.
class FrameRateGate final {
  public:
    void setTarget(int framesPerSecond) noexcept;

    [[nodiscard]] int getTarget() const noexcept {
        return target;
    }

    // Whether the refresh at the time plays a frame, which then schedules the next one.
    [[nodiscard]] bool takeFrame(double now) noexcept;

    // The seconds from the time until a refresh may play the next frame, for a platform whose loop waits on its own instead of on the display.
    [[nodiscard]] double getWaitSeconds(double now) const noexcept;

  private:
    static constexpr double kEarlyShare = 0.25;

    int target = 0;
    std::optional<double> due;
};

} // namespace haylen::platform
