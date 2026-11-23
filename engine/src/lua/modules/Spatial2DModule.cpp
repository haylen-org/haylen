#include "lua/modules/Spatial2DModule.hpp"

#include "2d/spatial/Spatial2DLua.hpp"

namespace haylen::lua::modules {

void Spatial2DModule::install(core::Engine&, lua_State* L) {
    spatial2d::Spatial2DLua::install(L);
}

} // namespace haylen::lua::modules
