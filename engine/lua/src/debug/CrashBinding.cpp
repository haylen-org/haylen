#include "debug/CrashBinding.hpp"

#include <lua.hpp>

#include <string>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/plugins/DebugPlugin.hpp"

namespace haylen::lua::debug {

// Adds a breadcrumb with `addBreadcrumb(message, data)`, where the optional data is a table of JSON values.
int CrashBinding::addBreadcrumb(lua_State* L) {
    std::string message = lua::Stack::read<std::string>(L, 1);
    haylen::core::Json data = lua_isnoneornil(L, 2) ? haylen::core::Json::object() : lua::JsonConverter::read(L, 2);
    lua::Runtime::getEngine(L).getPlugin<haylen::plugins::DebugPlugin>().addBreadcrumb(std::move(message), std::move(data));
    return 0;
}

// Returns the kept breadcrumbs, oldest first, as `{time, message, data}`.
int CrashBinding::breadcrumbs(lua_State* L) {
    const haylen::core::RingBuffer<haylen::debug::Breadcrumb>& kept = lua::Runtime::getEngine(L).getPlugin<haylen::plugins::DebugPlugin>().getBreadcrumbs();
    lua_createtable(L, static_cast<int>(kept.size()), 0);
    for (std::size_t index = 0; index < kept.size(); ++index) {
        lua_createtable(L, 0, 3);
        lua::Stack::push(L, kept[index].time);
        lua_setfield(L, -2, "time");
        lua::Stack::push(L, kept[index].message);
        lua_setfield(L, -2, "message");
        lua::JsonConverter::push(L, kept[index].data);
        lua_setfield(L, -2, "data");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int CrashBinding::lastCrash(lua_State* L) {
    const haylen::core::Json& report = lua::Runtime::getEngine(L).getPlugin<haylen::plugins::DebugPlugin>().getLastCrash();
    if (report.is_null()) {
        lua_pushnil(L);
        return 1;
    }

    lua::JsonConverter::push(L, report);
    return 1;
}

void CrashBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"addBreadcrumb", &lua::Binding::native<&addBreadcrumb>},
        {"breadcrumbs", &lua::Binding::native<&breadcrumbs>},
        {"lastCrash", &lua::Binding::native<&lastCrash>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::debug
