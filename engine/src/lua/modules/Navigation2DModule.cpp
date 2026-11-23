#include "lua/modules/Navigation2DModule.hpp"

#include "2d/navigation/Navigation2DLua.hpp"

namespace haylen::lua::modules {

void Navigation2DModule::install(core::Engine&, lua_State* L) {
    navigation2d::Navigation2DLua::install(L);
}

} // namespace haylen::lua::modules
