#pragma once

namespace haylen::platform {

// The input events of GameActivity that the engine takes before `sokol_app` translates them: those of controllers and the scrolling of mice and touchpads.
class AndroidInputEvents final {
  public:
    // Takes an input event as the native event callback of `sokol_app` and returns whether the engine handled it. It runs on the frame thread.
    static bool take(const void* source);
};

} // namespace haylen::platform
