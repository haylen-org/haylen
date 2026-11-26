#pragma once

#include <cstdint>

namespace haylen::gpu {
class Device;
}

namespace haylen::platform::sdl {

// Paces the frames of an Apple app from the main run loop, whose thread then waits between the frames as the main thread of every native app does, while the main callbacks of SDL wait for events. While the window shows, a frame comes when the GPU device offers the next drawable of the window through the display link of its layer, or at a refresh of a display link of the pacer where the device has none, at the rate of the app or at every refresh. While a macOS window shows nothing, which no display paces, a frame comes from a timer, and UIKit stops the display links of an app in the background by itself. Each frame wakes the main callbacks with an event, and the iteration after it plays the frame.
class AppleFramePacer final {
  public:
    // Starts the frames, which wake the main callbacks with events of the type, whose arrival the runtime reports with `receiveWake`.
    static void start(std::uint32_t wakeType);

    // Paces at the frames per second, or at every refresh for 0, through a display link while the window shows, and at the hidden rate through the timer while a macOS window shows nothing or no screen shows it.
    static void pace(int rate, int hiddenRate, bool hidden);

    // Paces the window through the device, which the runtime hands every device that draws the window as it makes it, and nothing once that device is gone.
    static void followDevice(gpu::Device* device);

    // Moves the display link onto the screen that the window shows on now.
    static void followScreen();

    static void receiveWake();

    // Whether a frame came since the runtime took the last one, which it takes.
    [[nodiscard]] static bool takeFrame();

    static void stop();
};

} // namespace haylen::platform::sdl
