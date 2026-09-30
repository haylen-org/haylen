#include "storage/PreferencesLua.hpp"

#include <lua.hpp>

#include <string_view>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/plugins/StoragePlugin.hpp"

namespace haylen::storage {

plugins::StoragePlugin& PreferencesLua::getPlugin(lua_State* L) {
    return lua::Runtime::getEngine(L).getPlugin<plugins::StoragePlugin>();
}

// Reads a preference with `get(key[, default])`.
int PreferencesLua::get(lua_State* L) {
    const core::Json defaultValue = lua_isnoneornil(L, 2) ? core::Json() : lua::JsonConverter::read(L, 2);
    lua::JsonConverter::push(L, getPlugin(L).getPreferences().get(lua::Stack::read<std::string_view>(L, 1), defaultValue));
    return 1;
}

int PreferencesLua::set(lua_State* L) {
    getPlugin(L).getPreferences().set(lua::Stack::read<std::string_view>(L, 1), lua::JsonConverter::read(L, 2));
    return 0;
}

int PreferencesLua::has(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).getPreferences().has(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int PreferencesLua::remove(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).getPreferences().remove(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int PreferencesLua::clear(lua_State* L) {
    getPlugin(L).getPreferences().clear();
    return 0;
}

int PreferencesLua::save(lua_State* L) {
    getPlugin(L).getPreferences().save();
    return 0;
}

int PreferencesLua::load(lua_State* L) {
    getPlugin(L).getPreferences().load();
    return 0;
}

int PreferencesLua::dirty(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).getPreferences().isDirty());
    return 1;
}

int PreferencesLua::values(lua_State* L) {
    lua::JsonConverter::push(L, getPlugin(L).getPreferences().getValues());
    return 1;
}

int PreferencesLua::capture(lua_State* L) {
    getPlugin(L).captureEnginePreferences(lua::Runtime::getEngine(L));
    return 0;
}

int PreferencesLua::apply(lua_State* L) {
    getPlugin(L).applyEnginePreferences(lua::Runtime::getEngine(L));
    return 0;
}

int PreferencesLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"get", &lua::Binding::native<&get>}, {"set", &lua::Binding::native<&set>}, {"has", &lua::Binding::native<&has>}, {"remove", &lua::Binding::native<&remove>}, {"clear", &lua::Binding::native<&clear>}, {"save", &lua::Binding::native<&save>}, {"load", &lua::Binding::native<&load>}, {"dirty", &lua::Binding::native<&dirty>}, {"values", &lua::Binding::native<&values>}, {"capture", &lua::Binding::native<&capture>}, {"apply", &lua::Binding::native<&apply>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void PreferencesLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.preferences", &open);
}

} // namespace haylen::storage
