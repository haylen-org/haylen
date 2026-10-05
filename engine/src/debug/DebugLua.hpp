#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::plugins {
class DebugPlugin;
}

namespace haylen::debug {

class Stats;

// Installs `haylen.debug`, which switches the statistics display, reads the statistics, frame times and profiler scopes, profiles Lua code, adds monitors, turns object events on and returns recent log lines.
class DebugLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<const char*, 4> kLevelNames{"debug", "info", "warning", "error"};
    static constexpr std::array<std::string_view, 1> kOwnerFields{"owner"};

    [[nodiscard]] static plugins::DebugPlugin& getPlugin(lua_State* L);
    static void pushStats(lua_State* L, const Stats& snapshot);

    static int setStatsMode(lua_State* L);
    static int statsMode(lua_State* L);
    static int setToggleKey(lua_State* L);
    static int toggleKey(lua_State* L);
    static int setDrawing(lua_State* L);
    static int drawing(lua_State* L);
    static int drawings(lua_State* L);
    static int drawingNames(lua_State* L);
    static int setDrawKey(lua_State* L);
    static int drawKey(lua_State* L);
    static int addDrawer(lua_State* L);
    static int setObjectEvents(lua_State* L);
    static int objectEvents(lua_State* L);
    static int stats(lua_State* L);
    static int addMonitor(lua_State* L);
    static int removeMonitor(lua_State* L);
    static int monitors(lua_State* L);
    static int frame(lua_State* L);
    static int frameHistory(lua_State* L);
    static int beginScope(lua_State* L);
    static int endScope(lua_State* L);
    static int profile(lua_State* L);
    static int recentLog(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::debug
