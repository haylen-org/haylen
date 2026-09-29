#pragma once

#include <array>
#include <string>
#include <string_view>

#include "haylen/core/Log.hpp"

struct lua_State;

namespace haylen::core {

// Installs the haylen module with the engine version, platform and configuration, the clock, the pause, the app state, the lifecycle options, autoloads and classes, and haylen.log, haylen.window and haylen.viewport.
class CoreLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kLifecycleFields{"pauseOnBackground", "pauseOnFocusLoss", "muteOnFocusLoss"};

    [[nodiscard]] static std::string joinArguments(lua_State* L);
    template <Log::Level Level> static int logMessage(lua_State* L);
    static int openLog(lua_State* L);

    static int rootQuit(lua_State* L);
    static int rootTime(lua_State* L);
    static int rootDelta(lua_State* L);
    static int rootUnscaledDelta(lua_State* L);
    static int rootFrame(lua_State* L);
    static int rootTimeScale(lua_State* L);
    static int rootSetTimeScale(lua_State* L);
    static int rootFixedStep(lua_State* L);
    static int rootInterpolation(lua_State* L);
    static int rootReportError(lua_State* L);
    static int rootPaused(lua_State* L);
    static int rootSetPaused(lua_State* L);
    static int rootAppState(lua_State* L);
    static int rootNetworkState(lua_State* L);
    static int rootHalted(lua_State* L);
    static int rootLifecycle(lua_State* L);
    static int rootSetLifecycle(lua_State* L);
    static int rootAutoload(lua_State* L);
    static int openRoot(lua_State* L);

    static int windowSize(lua_State* L);
    static int windowDpiScale(lua_State* L);
    static int windowFullscreen(lua_State* L);
    static int windowSetFullscreen(lua_State* L);
    static int windowResizable(lua_State* L);
    static int windowSetResizable(lua_State* L);
    static int windowSetTitle(lua_State* L);
    static int windowSetCursor(lua_State* L);
    static int windowSetCursorVisible(lua_State* L);
    static int windowSetMouseLocked(lua_State* L);
    static int windowShowKeyboard(lua_State* L);
    static int windowOrientation(lua_State* L);
    static int windowLockOrientation(lua_State* L);
    static int windowClipboard(lua_State* L);
    static int windowSetClipboard(lua_State* L);
    static int windowHasPointerDevice(lua_State* L);
    static int windowBackLeavesApp(lua_State* L);
    static int windowSetBackLeavesApp(lua_State* L);
    static int openWindow(lua_State* L);

    static int viewportDesignSize(lua_State* L);
    static int viewportVisibleRect(lua_State* L);
    static int viewportSafeRect(lua_State* L);
    static int viewportPixelRect(lua_State* L);
    static int viewportPixelsPerUnit(lua_State* L);
    static int viewportToDesign(lua_State* L);
    static int viewportToFramebuffer(lua_State* L);
    static int viewportScaling(lua_State* L);
    static int viewportSafeAreaSimulation(lua_State* L);
    static int viewportSetSafeAreaSimulation(lua_State* L);
    static int openViewport(lua_State* L);
};

} // namespace haylen::core
