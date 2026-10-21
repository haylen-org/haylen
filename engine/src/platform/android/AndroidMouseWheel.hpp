#pragma once

#include <game-activity/GameActivity.h>

namespace haylen::platform {

// The wheels of mice and the scrolling of touchpads on Android, which GameActivity reports as scroll motion events that the engine takes before the backend translates the others.
class AndroidMouseWheel final {
  public:
    // GameActivity copies the scroll axes of motion events only once they are enabled.
    static void enableAxes();

    // Turns a scroll motion event into a move of the mouse to where it scrolls and a scroll in steps of a wheel, and returns whether it was one. It runs on the frame thread.
    static bool handleEvent(const GameActivityMotionEvent& event);
};

} // namespace haylen::platform
