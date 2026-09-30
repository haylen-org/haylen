#include "lua/JobsLua.hpp"

#include <lua.hpp>

#include <chrono>
#include <cmath>
#include <stdexcept>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "plugins/JobsPlugin.hpp"

namespace haylen::lua {

plugins::JobsPlugin& JobsLua::getPlugin(lua_State* L) {
    return Runtime::getEngine(L).getPlugin<plugins::JobsPlugin>();
}

bool JobsLua::isCheckpoint(lua_State* job, int results) {
    return results == 1 && lua_touserdata(job, -1) == &checkpointKey;
}

// Runs `fn(...)` as a job with `spawn(fn, ...)` and returns a promise for the first value `fn` returns.
int JobsLua::spawn(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    const Promise promise(Runtime::getEngine(L));
    getPlugin(L).spawn(L, lua_gettop(L) - 1, promise);
    promise.push(L);
    return 1;
}

// Pauses the running job until the next frame once this frame's budget is spent, and returns right away otherwise. It yields itself, so it stays outside the exception guard.
int JobsLua::checkpoint(lua_State* L) {
    const int outOfTime = Binding::guarded(L, [L] { return getPlugin(L).isOutOfTime(L) ? 1 : 0; });
    if (outOfTime == 0) {
        return 0;
    }
    lua_pushlightuserdata(L, &checkpointKey);
    return lua_yield(L, 1);
}

// A budget beyond what the clock counts, such as `math.huge`, lets the jobs run without a limit.
int JobsLua::setBudget(lua_State* L) {
    const double microseconds = Stack::read<double>(L, 1) * 1000.0;
    if (!(microseconds > 0.0)) {
        throw std::invalid_argument("The job budget must be a positive number of milliseconds.");
    }
    const auto largest = static_cast<double>(std::chrono::microseconds::max().count());
    getPlugin(L).setBudget(microseconds >= largest ? std::chrono::microseconds::max() : std::chrono::microseconds(std::llround(microseconds)));
    return 0;
}

int JobsLua::budget(lua_State* L) {
    Stack::push(L, static_cast<double>(getPlugin(L).getBudget().count()) / 1000.0);
    return 1;
}

int JobsLua::runningCount(lua_State* L) {
    Stack::push(L, getPlugin(L).getRunningCount());
    return 1;
}

int JobsLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"spawn", &Binding::native<&spawn>}, {"checkpoint", &checkpoint}, {"setBudget", &Binding::native<&setBudget>}, {"budget", &budget}, {"runningCount", &runningCount}, {nullptr, nullptr},
    };
    Binding::newModule(L, functions);
    return 1;
}

void JobsLua::install(lua_State* L) {
    Binding::preload(L, "haylen.jobs", &open);
}

} // namespace haylen::lua
