#pragma once

#include "backends/sokol/Window.hpp"
#include "sokol_app.h"

namespace haylen::sokol {

// Plays apps in the window of `sokol_app`, which calls the player of the platform as the window opens, draws its frames, receives its events and closes.
class Runtime final {
  public:
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    // Plays the command line until the window closes. Apple apps enter here through `haylen_main`, because `sokol_app` leaves main to them, and Windows and Linux processes through the entry point of the runtime, which exits with the status this returns. AppKit and UIKit end the process themselves instead.
    static int run(int argc, char* argv[]);
#endif

    // Creates the player for the command line and describes the window of its app to `sokol_app`.
    [[nodiscard]] static sapp_desc describe(int argc, char* argv[]);

  private:
    // The window of the process, which services on other threads may read while it exits.
    [[nodiscard]] static Window& getWindow() noexcept;

    static void onInitialize();
    static void onFrame();
    static void onEvent(const sapp_event* source);
    static void onCleanup();
#if defined(__ANDROID__)
    // The input events of GameActivity that the engine takes before `sokol_app` translates them, and the keys of controllers that the app takes from the views and the system.
    static bool onNativeEvent(const void* source);
    static bool onKeyConsumed(const void* source);
#endif
};

} // namespace haylen::sokol
