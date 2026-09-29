#include "haylen/lua/NativeProperty.hpp"

namespace haylen::lua {

const NativeProperty* NativeProperty::find(lua_State* L, int index, std::string_view name) {
    if (lua_type(L, index) != LUA_TUSERDATA || lua_getmetatable(L, index) == 0) {
        return nullptr;
    }
    if (lua_getfield(L, -1, kField) != LUA_TTABLE) {
        lua_pop(L, 2);
        return nullptr;
    }
    lua_pushlstring(L, name.data(), name.size());
    lua_rawget(L, -2);
    const auto* property = static_cast<const NativeProperty*>(lua_touserdata(L, -1));
    lua_pop(L, 3);
    return property;
}

void NativeProperty::record(lua_State* L, int metatable, const char* name, NativeProperty& property) {
    if (lua_getfield(L, metatable, kField) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, metatable, kField);
    }
    lua_pushlightuserdata(L, &property);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

} // namespace haylen::lua
