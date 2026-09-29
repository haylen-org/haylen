#include "lua/WeakReference.hpp"

#include "haylen/lua/Runtime.hpp"

namespace haylen::lua {

void WeakReference::pushTable(lua_State* L) {
    if (lua_getfield(L, LUA_REGISTRYINDEX, kTable) == LUA_TTABLE) {
        return;
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_createtable(L, 0, 1);
    lua_pushliteral(L, "v");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, kTable);
}

// Keys come from a counter in slot 0 of the weak table, which numbers never leave.
WeakReference::WeakReference(lua_State* L, int index) : state(Runtime::getMainThread(L)), identity(lua_topointer(L, index)) {
    const int value = lua_absindex(L, index);
    const int type = lua_type(L, value);
    if (type != LUA_TTABLE && type != LUA_TUSERDATA) {
        luaL_error(L, "Only a table or a userdata can be referenced weakly, not %s.", luaL_typename(L, value));
    }
    pushTable(L);
    lua_rawgeti(L, -1, 0);
    key = lua_tointeger(L, -1) + 1;
    lua_pop(L, 1);
    lua_pushinteger(L, key);
    lua_rawseti(L, -2, 0);
    lua_pushvalue(L, value);
    lua_rawseti(L, -2, key);
    lua_pop(L, 1);
}

WeakReference::~WeakReference() {
    pushTable(state);
    lua_pushnil(state);
    lua_rawseti(state, -2, key);
    lua_pop(state, 1);
}

bool WeakReference::push(lua_State* L) const {
    pushTable(L);
    const bool present = lua_rawgeti(L, -1, key) != LUA_TNIL;
    lua_remove(L, -2);
    if (!present) {
        lua_pop(L, 1);
    }
    return present;
}

bool WeakReference::isAlive() const {
    if (!push(state)) {
        return false;
    }
    lua_pop(state, 1);
    return true;
}

} // namespace haylen::lua
