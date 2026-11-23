#include "runtime/JobsBinding.hpp"

#include <lua.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "runtime/Jobs.hpp"
#include "runtime/Owners.hpp"

namespace haylen::lua {

Jobs& JobsBinding::getJobs(lua_State* L) {
    return Runtime::getEngine(L).getPlugin<ScriptPlugin>().getJobs();
}

bool JobsBinding::isCheckpoint(lua_State* job, int results) {
    return results == 1 && lua_touserdata(job, -1) == &checkpointKey;
}

// Runs `fn` as a job with `spawn(fn, options)` and returns a promise for the first value `fn` returns. A job without an `owner` belongs to the default owner.
int JobsBinding::spawn(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    int owner = 0;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        Table::checkFields(L, 2, {kSpawnFields});
        if (lua_getfield(L, 2, "owner") != LUA_TNIL) {
            Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }

    if (owner == 0) {
        owner = Owners::pushDefault(L);
    }

    const Promise promise(Runtime::getEngine(L));
    lua_pushvalue(L, 1);
    getJobs(L).spawn(L, owner, promise);
    promise.push(L);
    return 1;
}

// Pauses the running job until the next frame once this frame's budget is spent, and returns right away otherwise. It yields itself, so it stays outside the exception guard.
int JobsBinding::checkpoint(lua_State* L) {
    const int outOfTime = Binding::guarded(L, [L] { return getJobs(L).isOutOfTime(L) ? 1 : 0; });
    if (outOfTime == 0) {
        return 0;
    }

    lua_pushlightuserdata(L, &checkpointKey);
    return lua_yield(L, 1);
}

// A budget beyond what the clock counts, such as `math.huge`, lets the jobs run without a limit, and a positive budget below a microsecond gets one.
int JobsBinding::setBudget(lua_State* L) {
    const double microseconds = Stack::read<double>(L, 1) * 1000.0;
    if (!(microseconds > 0.0)) {
        throw std::invalid_argument("The job budget must be a positive number of milliseconds.");
    }

    const auto largest = static_cast<double>(std::chrono::microseconds::max().count());
    const std::chrono::microseconds limit = microseconds >= largest ? std::chrono::microseconds::max() : std::chrono::microseconds(std::max(1LL, std::llround(microseconds)));
    getJobs(L).setBudget(limit);
    return 0;
}

int JobsBinding::budget(lua_State* L) {
    Stack::push(L, static_cast<double>(getJobs(L).getBudget().count()) / 1000.0);
    return 1;
}

int JobsBinding::runningCount(lua_State* L) {
    Stack::push(L, getJobs(L).getRunningCount());
    return 1;
}

int JobsBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"spawn", &Binding::native<&spawn>}, {"checkpoint", &checkpoint}, {"setBudget", &Binding::native<&setBudget>}, {"budget", &Binding::native<&budget>}, {"runningCount", &Binding::native<&runningCount>}, {nullptr, nullptr},
    };
    Binding::newModule(L, functions);
    return 1;
}

void JobsBinding::install(lua_State* L) {
    Binding::preload(L, "haylen.jobs", &open);
}

} // namespace haylen::lua
