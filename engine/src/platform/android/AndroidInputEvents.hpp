#pragma once

#include <game-activity/GameActivity.h>

namespace haylen::platform {

// The input events of GameActivity that the engine takes before the backend translates them: those of controllers and the scrolling of mice and touchpads.
class AndroidInputEvents final {
  public:
    // Take a key or a motion event and return whether the engine handled it. They run on the frame thread.
    static bool takeKey(const GameActivityKeyEvent& event);
    static bool takeMotion(const GameActivityMotionEvent& event);

    // Answers on the UI thread whether the app takes a key from the views and the system, which it does for the buttons of controllers.
    static bool takesKey(const GameActivityKeyEvent& event);
};

} // namespace haylen::platform
