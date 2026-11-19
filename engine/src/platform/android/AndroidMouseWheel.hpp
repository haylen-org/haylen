#pragma once

#include <game-activity/GameActivity.h>

namespace haylen::platform {

// The wheels of mice and the scrolling of touchpads on Android, which GameActivity reports as scroll motion events.
class AndroidMouseWheel final {
  public:
    // GameActivity copies the scroll axes of motion events only once they are enabled.
    static void enableAxes();

    // Turns a scroll motion event into a move of the mouse to where it scrolls and a scroll in steps of a wheel, and returns whether it was one. It runs on the frame thread.
    static bool handleEvent(const GameActivityMotionEvent& event);

    // Posts the two-finger swipe of a touchpad, which Android 14 and later hand over as one fake finger, as the phased scroll of a precise device at its place in pixels of the window, by a distance in points. It runs on the UI thread.
    static void receiveTouchpadScroll(int phase, float x, float y, float deltaX, float deltaY);
};

} // namespace haylen::platform
