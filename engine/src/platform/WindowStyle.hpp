#pragma once

namespace haylen::platform {

// The desktop options of the window that each platform turns into its decorations, window levels and window manager hints at once, because they depend on each other there.
struct WindowStyle {
    bool transparent = false;
    bool resizable = true;
    bool decorated = true;
    bool alwaysOnTop = false;
    bool shownInTaskbar = true;
};

} // namespace haylen::platform
