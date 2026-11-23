#include "2d/navigation/Navigation2DBinding.hpp"

#include <lua.hpp>

#include "2d/navigation/AgentBinding.hpp"
#include "2d/navigation/NavGraphBinding.hpp"
#include "2d/navigation/NavGridBinding.hpp"
#include "2d/navigation/NavMeshBinding.hpp"
#include "haylen/lua/Binding.hpp"

namespace haylen::lua::navigation2d {

int Navigation2DBinding::open(lua_State* L) {
    lua_newtable(L);
    NavGridBinding::addFunctions(L);
    NavGraphBinding::addFunctions(L);
    NavMeshBinding::addFunctions(L);
    AgentBinding::addFunctions(L);
    return 1;
}

void Navigation2DBinding::install(lua_State* L) {
    NavGridBinding::install(L);
    NavGraphBinding::install(L);
    NavMeshBinding::install(L);
    AgentBinding::install(L);
    lua::Binding::preload(L, "haylen.navigation2d", &open);
}

} // namespace haylen::lua::navigation2d
