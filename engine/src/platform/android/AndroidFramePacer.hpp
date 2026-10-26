#pragma once

#include <optional>

namespace haylen::platform {

// Decides when the frame thread of an Android activity plays the app. A resumed activity with a window draws a frame for each refresh of the display, also while a dialog or the notification shade has the focus of its window. A paused activity draws nothing, but ticks a started app ten times a second, so its event loop, timers and callbacks go on under another activity and in the background. Times are seconds of the monotonic clock. It compiles on every platform, so the tests of every host check it.
class AndroidFramePacer final {
  public:
    static constexpr double kTickSeconds = 0.1;

    void setResumed(bool value) noexcept {
        resumed = value;
    }

    void setWindow(bool value) noexcept {
        window = value;
    }

    // Whether the app plays, from the first frame of the activity until the app ends.
    void setStarted(bool value) noexcept {
        started = value;
    }

    [[nodiscard]] bool isDrawing() const noexcept {
        return resumed && window;
    }

    // Whether the tick of a paused app is due at the time, which then schedules the next one.
    [[nodiscard]] bool takeTick(double now) noexcept;

    // How many milliseconds the frame thread may wait for messages until the next tick, or -1 while the app does not tick.
    [[nodiscard]] int getWaitMilliseconds(double now) const noexcept;

    // The seconds since the frame or the tick before, which the first one counts as none.
    [[nodiscard]] double takeElapsed(double now) noexcept;

  private:
    [[nodiscard]] bool isTicking() const noexcept {
        return started && !resumed;
    }

    std::optional<double> last;
    double nextTick = 0.0;
    bool resumed = false;
    bool window = false;
    bool started = false;
};

} // namespace haylen::platform
