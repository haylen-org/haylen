#pragma once

#include <array>
#include <string>
#include <string_view>

#include "haylen/core/Log.hpp"

struct lua_State;

namespace haylen::core {

// Installs the `haylen` module with the engine version, platform and configuration, the clock, the pause, the app state, the lifecycle options, autoloads and classes, and `haylen.log` and `haylen.viewport`.
class CoreLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 1> kQuitFields{"status"};
    static constexpr std::array<std::string_view, 3> kLifecycleFields{"pauseOnBackground", "pauseOnFocusLoss", "muteOnFocusLoss"};

    [[nodiscard]] static std::string joinArguments(lua_State* L);
    template <Log::Level Level> static int logMessage(lua_State* L);
    static int openLog(lua_State* L);

    static int rootQuit(lua_State* L);
    static int rootRequestRestart(lua_State* L);
    static int rootElapsed(lua_State* L);
    static int rootDelta(lua_State* L);
    static int rootUnscaledDelta(lua_State* L);
    static int rootFrameIndex(lua_State* L);
    static int rootTimeScale(lua_State* L);
    static int rootSetTimeScale(lua_State* L);
    static int rootFixedStep(lua_State* L);
    static int rootInterpolation(lua_State* L);
    static int rootReportError(lua_State* L);
    static int rootRecoverable(lua_State* L);
    static int rootSetRecoverable(lua_State* L);
    static int rootRecover(lua_State* L);
    static int rootPaused(lua_State* L);
    static int rootSetPaused(lua_State* L);
    static int rootAppState(lua_State* L);
    static int rootAppCovered(lua_State* L);
    static int rootNetworkState(lua_State* L);
    static int rootHalted(lua_State* L);
    static int rootLifecycle(lua_State* L);
    static int rootSetLifecycle(lua_State* L);
    static int rootAutoload(lua_State* L);
    static int openRoot(lua_State* L);

    static int viewportDesignSize(lua_State* L);
    static int viewportVisibleRect(lua_State* L);
    static int viewportSafeRect(lua_State* L);
    static int viewportReservedInsets(lua_State* L);
    static int viewportPixelRect(lua_State* L);
    static int viewportPixelsPerUnit(lua_State* L);
    static int viewportToDesign(lua_State* L);
    static int viewportToFramebuffer(lua_State* L);
    static int viewportScaling(lua_State* L);
    static int viewportSetScaling(lua_State* L);
    static int viewportSetDesignSize(lua_State* L);
    static int viewportSafeAreaSimulation(lua_State* L);
    static int viewportSetSafeAreaSimulation(lua_State* L);
    static int openViewport(lua_State* L);
};

} // namespace haylen::core
