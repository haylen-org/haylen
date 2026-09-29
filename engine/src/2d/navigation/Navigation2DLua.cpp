#include "2d/navigation/Navigation2DLua.hpp"

#include <lua.hpp>

#include "2d/navigation/AgentLua.hpp"
#include "2d/navigation/NavGraphLua.hpp"
#include "2d/navigation/NavGridLua.hpp"
#include "2d/navigation/NavMeshLua.hpp"
#include "haylen/lua/Binding.hpp"

namespace haylen::navigation2d {

int Navigation2DLua::open(lua_State* L) {
    lua_newtable(L);
    NavGridLua::addFunctions(L);
    NavGraphLua::addFunctions(L);
    NavMeshLua::addFunctions(L);
    AgentLua::addFunctions(L);
    return 1;
}

void Navigation2DLua::install(lua_State* L) {
    NavGridLua::install(L);
    NavGraphLua::install(L);
    NavMeshLua::install(L);
    AgentLua::install(L);
    lua::Binding::preload(L, "haylen.navigation2d", &open);
}

} // namespace haylen::navigation2d
