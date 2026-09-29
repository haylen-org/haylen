#include "haylen/lua/Binding.hpp"

namespace haylen::lua {

void Binding::preload(lua_State* L, const char* name, lua_CFunction opener) {
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    lua_pushcfunction(L, opener);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

void Binding::newModule(lua_State* L, const luaL_Reg* functions) {
    lua_newtable(L);
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua
