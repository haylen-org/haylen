#include "debug/InspectorLua.hpp"

#include <lua.hpp>

#include <string>
#include <vector>

#include "debug/Inspector.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/plugins/DebugPlugin.hpp"

namespace haylen::debug {

// Exposes a value under `watched` with `watch(name, value)`, held weakly.
int InspectorLua::watch(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 1);
    luaL_checkany(L, 2);
    Inspector::watch(L, name, 2);
    return 0;
}

int InspectorLua::unwatch(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 1);
    lua_pushnil(L);
    Inspector::watch(L, name, -1);
    return 0;
}

// Returns `{key, type, value, editable, expandable}` for every value at a path such as `'watched.player'`, or the roots without one.
int InspectorLua::inspect(lua_State* L) {
    const std::vector<std::string> path = Inspector::splitPath(lua_isnoneornil(L, 1) ? std::string_view() : lua::Stack::read<std::string_view>(L, 1));
    const std::vector<Inspector::Entry> entries = Inspector::list(L, path);
    lua_createtable(L, static_cast<int>(entries.size()), 0);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const Inspector::Entry& entry = entries[index];
        lua_createtable(L, 0, 5);
        lua::Stack::push(L, entry.key);
        lua_setfield(L, -2, "key");
        lua::Stack::push(L, entry.type);
        lua_setfield(L, -2, "type");
        lua::Stack::push(L, entry.value);
        lua_setfield(L, -2, "value");
        lua::Stack::push(L, entry.editable);
        lua_setfield(L, -2, "editable");
        lua::Stack::push(L, entry.expandable);
        lua_setfield(L, -2, "expandable");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int InspectorLua::setInspectorVisible(lua_State* L) {
    lua::Runtime::getEngine(L).getPlugin<plugins::DebugPlugin>().setInspectorVisible(lua::Stack::read<bool>(L, 1));
    return 0;
}

int InspectorLua::inspectorVisible(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPlugin<plugins::DebugPlugin>().isInspectorVisible());
    return 1;
}

void InspectorLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"watch", &lua::Binding::native<&watch>}, {"unwatch", &lua::Binding::native<&unwatch>}, {"inspect", &lua::Binding::native<&inspect>}, {"setInspectorVisible", &lua::Binding::native<&setInspectorVisible>}, {"inspectorVisible", &lua::Binding::native<&inspectorVisible>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::debug
