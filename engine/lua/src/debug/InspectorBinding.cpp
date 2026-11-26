#include "debug/InspectorBinding.hpp"

#include <lua.hpp>

#include <optional>
#include <string>
#include <vector>

#include "debug/PickRegistry.hpp"
#include "debug/ScriptInspector.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/debug/Inspector.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/plugins/DebugPlugin.hpp"

namespace haylen::lua::debug {

// Exposes a value under `watched` with `watch(name, value)`, held weakly.
int InspectorBinding::watch(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 1);
    luaL_checkany(L, 2);
    ScriptInspector::watch(L, name, 2);
    return 0;
}

int InspectorBinding::unwatch(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 1);
    lua_pushnil(L);
    ScriptInspector::watch(L, name, -1);
    return 0;
}

// Returns `{key, type, value, editable, expandable}` for every value at a path such as `'watched.player'`, or the roots without one.
int InspectorBinding::inspect(lua_State* L) {
    const std::string_view text = lua_isnoneornil(L, 1) ? std::string_view() : lua::Stack::read<std::string_view>(L, 1);
    const std::vector<std::string> path = haylen::debug::Inspector::splitPath(text);
    haylen::debug::Inspector::Listing listing;
    ScriptInspector(L).list(path, listing);
    const std::vector<haylen::debug::Inspector::Entry>& entries = listing.entries;
    lua_createtable(L, static_cast<int>(entries.size()), 0);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const haylen::debug::Inspector::Entry& entry = entries[index];
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

int InspectorBinding::setInspectorVisible(lua_State* L) {
    lua::Runtime::getEngine(L).getPlugin<haylen::plugins::DebugPlugin>().setInspectorVisible(lua::Stack::read<bool>(L, 1));
    return 0;
}

int InspectorBinding::inspectorVisible(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPlugin<haylen::plugins::DebugPlugin>().isInspectorVisible());
    return 1;
}

// Picks the key that shows and hides the inspector with `setInspectorKey(name)`, or turns the shortcut off with `nil`.
int InspectorBinding::setInspectorKey(lua_State* L) {
    const std::optional<haylen::input::Key> key = lua_isnoneornil(L, 1) ? std::nullopt : std::optional<haylen::input::Key>(lua::Stack::read<haylen::input::Key>(L, 1));
    lua::Runtime::getEngine(L).getPlugin<haylen::plugins::DebugPlugin>().setInspectorKey(key);
    return 0;
}

int InspectorBinding::inspectorKey(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPlugin<haylen::plugins::DebugPlugin>().getInspectorKey());
    return 1;
}

int InspectorBinding::setPicking(lua_State* L) {
    lua::Runtime::getEngine(L).getPlugin<haylen::plugins::DebugPlugin>().setPicking(lua::Stack::read<bool>(L, 1));
    return 0;
}

int InspectorBinding::picking(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPlugin<haylen::plugins::DebugPlugin>().isPicking());
    return 1;
}

// Returns what the pick on the screen selected with `selected()`, the value of the root `selected` of the inspector, or nil.
int InspectorBinding::selected(lua_State* L) {
    PickRegistry::pushSelected(L);
    return 1;
}

void InspectorBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"watch", &lua::Binding::native<&watch>}, {"unwatch", &lua::Binding::native<&unwatch>}, {"inspect", &lua::Binding::native<&inspect>}, {"setInspectorVisible", &lua::Binding::native<&setInspectorVisible>}, {"inspectorVisible", &lua::Binding::native<&inspectorVisible>}, {"setInspectorKey", &lua::Binding::native<&setInspectorKey>}, {"inspectorKey", &lua::Binding::native<&inspectorKey>}, {"setPicking", &lua::Binding::native<&setPicking>}, {"picking", &lua::Binding::native<&picking>}, {"selected", &lua::Binding::native<&selected>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::debug
