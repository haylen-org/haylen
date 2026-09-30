#pragma once

struct lua_State;

namespace haylen::plugins {
class JobsPlugin;
}

namespace haylen::lua {

// Installs `haylen.jobs`, which starts Lua jobs and lets them pause at checkpoints.
class JobsLua final {
  public:
    static void install(lua_State* L);

    // Tells whether a job yielded the results on top of its stack from `jobs.checkpoint`.
    [[nodiscard]] static bool isCheckpoint(lua_State* job, int results);

  private:
    // Its address marks the yields that come from `jobs.checkpoint`.
    inline static char checkpointKey = 0;

    [[nodiscard]] static plugins::JobsPlugin& getPlugin(lua_State* L);
    static int spawn(lua_State* L);
    static int checkpoint(lua_State* L);
    static int setBudget(lua_State* L);
    static int budget(lua_State* L);
    static int runningCount(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::lua
