#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#include <cstdint>

namespace haylen::platform::sdl {

// Paces the frames of a macOS app from the main run loop, whose thread then waits between the frames as the main thread of every native app does, while the main callbacks of SDL wait for events. While the window shows, a frame comes at a refresh of the display link of the screen of the window, at the rate of the app or at every refresh, and while the window shows nothing, which no display paces, a frame comes from a timer. Each frame wakes the main callbacks with an event, and the iteration after it plays the frame.
class AppleFramePacer final {
  public:
    // Starts the frames, which wake the main callbacks with events of the type, whose arrival the runtime reports with `receiveWake`.
    static void start(std::uint32_t wakeType);

    // Paces at the frames per second, or at every refresh for 0, through the display link while the window shows, and at the hidden rate through the timer while it shows nothing or no screen shows it.
    static void pace(int rate, int hiddenRate, bool hidden);

    // Moves the display link onto the screen that the window shows on now.
    static void followScreen();

    static void receiveWake();

    // Whether a frame came since the runtime took the last one, which it takes.
    [[nodiscard]] static bool takeFrame();

    static void stop();
};

} // namespace haylen::platform::sdl
#endif
