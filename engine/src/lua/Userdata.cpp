#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

bool Userdata::newMetatable(lua_State* L, const char* name) {
    if (luaL_newmetatable(L, name) == 0) {
        return false;
    }
    lua_pushstring(L, name);
    lua_setfield(L, -2, "__metatable");
    return true;
}

void Userdata::pushField(lua_State* L, int index, const char* name) {
    if (lua_getiuservalue(L, index, 1) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_pushnil(L);
        return;
    }
    lua_getfield(L, -1, name);
    lua_remove(L, -2);
}

void Userdata::setField(lua_State* L, int index, const char* name, int valueIndex) {
    const int owner = lua_absindex(L, index);
    const int value = lua_absindex(L, valueIndex);
    if (lua_getiuservalue(L, owner, 1) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setiuservalue(L, owner, 1);
    }
    lua_pushvalue(L, value);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

bool Userdata::pushFunction(lua_State* L, int index, const char* name) {
    pushField(L, index, name);
    if (lua_isfunction(L, -1)) {
        return true;
    }
    lua_pop(L, 1);
    return false;
}

} // namespace haylen::lua
