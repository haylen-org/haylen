#pragma once

namespace haylen::platform {

// Decides which frames of a window play and present where the window system tells, through a callback, that it took the last frame and would show a new one, as the frame callbacks of a Wayland compositor do. A frame presents only once the callback of the last frame that presented arrived, so the swapchain never waits for a window system that holds its callbacks, such as a compositor whose outputs sleep. While the callbacks come, every frame follows one, so the updates and the presents keep one rate on any refresh. Once no callback came for `kHeldSeconds`, the window system holds them, and each tick of a timer that keeps the app running plays a frame that presents nothing, until the next callback presents at once. A window that the window system suspends presents nothing until it shows again. Times are seconds of a monotonic clock, and it compiles on every platform, so the tests of every host check it.
class FrameCallbackGate final {
  public:
    // How long the callback of the last frame may take before the window system counts as holding the callbacks. The only sign that a callback is absent is time, and no refresh of a display or of a compositor that throttles a window comes that far apart, while a quarter of a second keeps the timers and the network of the app running without a gap that a person notices.
    static constexpr double kHeldSeconds = 0.25;

    // Whether the frame that plays now presents, which asks the window system for the callback of that frame.
    [[nodiscard]] bool takePresent(double now) noexcept;

    // The callback of the last frame that presented arrived, and the next frame presents at once.
    void receiveCallback() noexcept;

    // A tick of the timer, which plays a frame only while no frame waits for a callback or the window system holds the callbacks.
    [[nodiscard]] bool tick(double now) const noexcept;

    void setSuspended(bool value) noexcept {
        suspended = value;
    }

    [[nodiscard]] bool isSuspended() const noexcept {
        return suspended;
    }

    // Whether a frame that presented still waits for its callback.
    [[nodiscard]] bool isWaiting() const noexcept {
        return waiting;
    }

  private:
    bool waiting = false;
    double askedAt = 0.0;
    bool suspended = false;
};

} // namespace haylen::platform
