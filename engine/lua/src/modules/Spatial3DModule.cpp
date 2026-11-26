#include "modules/Spatial3DModule.hpp"

#include "3d/spatial/Spatial3DBinding.hpp"

namespace haylen::lua::modules {

void Spatial3DModule::install(haylen::core::Engine&, lua_State* L) {
    lua::spatial3d::Spatial3DBinding::install(L);
}

} // namespace haylen::lua::modules
