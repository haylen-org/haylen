#include "modules/Navigation2DModule.hpp"

#include "2d/navigation/Navigation2DBinding.hpp"

namespace haylen::lua::modules {

void Navigation2DModule::install(haylen::core::Engine&, lua_State* L) {
    lua::navigation2d::Navigation2DBinding::install(L);
}

} // namespace haylen::lua::modules
