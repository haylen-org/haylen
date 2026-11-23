#include "modules/Spatial2DModule.hpp"

#include "2d/spatial/Spatial2DBinding.hpp"

namespace haylen::lua::modules {

void Spatial2DModule::install(haylen::core::Engine&, lua_State* L) {
    lua::spatial2d::Spatial2DBinding::install(L);
}

} // namespace haylen::lua::modules
