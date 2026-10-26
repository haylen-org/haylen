#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>

#include <functional>

#include "haylen/platform/Event.hpp"

namespace haylen::platform {

// The scrolls of the mouse and the trackpads of macOS, read from AppKit with the phase of their gesture: precise devices move by points of the screen, wheels by their steps, and the scrolls that only change the phase, as the fingers touch or leave the trackpad, arrive too.
class AppleScroll final {
  public:
    // Hands every scroll over the window of the app that moves or changes its phase to the callback, on the main thread, as AppKit takes it from the queue of the app.
    static void watch(std::function<void(const Event&)> deliver);

  private:
    [[nodiscard]] static Event translate(NSEvent* event);
    [[nodiscard]] static Event::ScrollPhase toPhase(NSEvent* event);
};

} // namespace haylen::platform
#endif
