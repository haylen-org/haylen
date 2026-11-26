#include "3d/navigation/Navigation3DBinding.hpp"

#include <lua.hpp>

#include "3d/navigation/CrowdBinding.hpp"
#include "3d/navigation/GeometryBinding.hpp"
#include "3d/navigation/NavMeshBinding.hpp"
#include "3d/navigation/QueryFilterBinding.hpp"
#include "haylen/lua/Binding.hpp"

namespace haylen::lua::navigation3d {

int Navigation3DBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newGeometry", &lua::Binding::native<&GeometryBinding::create>}, {"build", &lua::Binding::native<&NavMeshBinding::build>}, {"buildAsync", &lua::Binding::native<&NavMeshBinding::buildAsync>}, {"decode", &lua::Binding::native<&NavMeshBinding::decode>}, {"newCrowd", &lua::Binding::native<&CrowdBinding::create>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void Navigation3DBinding::install(lua_State* L) {
    GeometryBinding::install(L);
    QueryFilterBinding::install(L);
    NavMeshBinding::install(L);
    CrowdBinding::install(L);
    lua::Binding::preload(L, "haylen.navigation3d", &open);
}

} // namespace haylen::lua::navigation3d
