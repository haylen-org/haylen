#pragma once

#include "haylen/math/Polygon.hpp"
#include "haylen/platform/Monitor.hpp"

struct lua_State;

namespace haylen::platform {

// Installs haylen.window, which controls the window or the canvas of the app: its size, fullscreen, title, cursor, on-screen keyboard, orientation, clipboard and back button, and on desktops its decorations, level, taskbar presence, focus, frame, monitors, dragging and mouse passthrough.
class WindowLua final {
  public:
    static void install(lua_State* L);

  private:
    [[nodiscard]] static bool isRect(lua_State* L, int index);
    [[nodiscard]] static math::Polygon::Outline readRegion(lua_State* L, int index);
    static void pushMonitor(lua_State* L, const Monitor& monitor);

    static int framebufferSize(lua_State* L);
    static int dpiScale(lua_State* L);
    static int fullscreen(lua_State* L);
    static int setFullscreen(lua_State* L);
    static int resizable(lua_State* L);
    static int setResizable(lua_State* L);
    static int setTitle(lua_State* L);
    static int setCursor(lua_State* L);
    static int setCursorVisible(lua_State* L);
    static int setMouseLocked(lua_State* L);
    static int setKeyboardVisible(lua_State* L);
    static int orientation(lua_State* L);
    static int lockOrientation(lua_State* L);
    static int clipboard(lua_State* L);
    static int setClipboard(lua_State* L);
    static int hasPointerDevice(lua_State* L);
    static int backLeavesApp(lua_State* L);
    static int setBackLeavesApp(lua_State* L);
    static int canBeTransparent(lua_State* L);
    static int transparent(lua_State* L);
    static int setTransparent(lua_State* L);
    static int decorated(lua_State* L);
    static int setDecorated(lua_State* L);
    static int alwaysOnTop(lua_State* L);
    static int setAlwaysOnTop(lua_State* L);
    static int showInTaskbar(lua_State* L);
    static int setShowInTaskbar(lua_State* L);
    static int focusable(lua_State* L);
    static int setFocusable(lua_State* L);
    static int frame(lua_State* L);
    static int setFrame(lua_State* L);
    static int place(lua_State* L);
    static int mousePassthrough(lua_State* L);
    static int setMousePassthrough(lua_State* L);
    static int startDrag(lua_State* L);
    static int monitors(lua_State* L);
    static int currentMonitor(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::platform
