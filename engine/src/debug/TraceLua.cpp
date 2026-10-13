#include "debug/TraceLua.hpp"

#include <lua.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/debug/TraceRecorder.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "haylen/storage/UserStorage.hpp"

namespace haylen::debug {

// Starts a trace with `startTrace({frames, capacity})`, which records every frame until it stops without `frames`.
int TraceLua::startTrace(lua_State* L) {
    std::optional<int> frames;
    lua_Integer capacity = static_cast<lua_Integer>(TraceRecorder::kDefaultCapacity);
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kTraceFields});
        if (lua_getfield(L, 1, "frames") != LUA_TNIL) {
            frames = static_cast<int>(luaL_checkinteger(L, -1));
        }
        if (lua_getfield(L, 1, "capacity") != LUA_TNIL) {
            capacity = luaL_checkinteger(L, -1);
            if (capacity < 1) {
                throw std::invalid_argument("A trace holds at least 1 span.");
            }
        }
    }
    lua::Runtime::getEngine(L).getPlugin<plugins::DebugPlugin>().startTrace(frames, static_cast<std::size_t>(capacity));
    return 0;
}

int TraceLua::tracing(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPlugin<plugins::DebugPlugin>().isTracing());
    return 1;
}

// Stops the trace with `stopTraceAsync(path)` and writes it to user storage on the I/O pool, resolving with `{path, spans, dropped}`.
int TraceLua::stopTraceAsync(lua_State* L) {
    std::string path = lua::Stack::read<std::string>(L, 1);
    core::Engine& engine = lua::Runtime::getEngine(L);
    std::shared_ptr<TraceRecorder> trace = engine.getPlugin<plugins::DebugPlugin>().stopTrace();
    if (!trace) {
        throw std::logic_error("No trace is recording. Start one with \"debug.startTrace\".");
    }
    const lua::Promise promise(engine);
    storage::UserStorage& files = engine.getStorage();
    // clang-format off
    engine.getJobs().postIo([promise, &files, path = std::move(path), trace = std::move(trace)] {
        try {
            files.writeText(path, trace->toJson());
            promise.resolve({{"path", path}, {"spans", trace->size()}, {"dropped", trace->getDropped()}});
        } catch (const std::exception& error) {
            promise.reject(error.what());
        }
    });
    // clang-format on
    promise.push(L);
    return 1;
}

void TraceLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"startTrace", &lua::Binding::native<&startTrace>},
        {"tracing", &lua::Binding::native<&tracing>},
        {"stopTraceAsync", &lua::Binding::native<&stopTraceAsync>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::debug
