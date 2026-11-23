#include "lua/modules/Physics2DModule.hpp"

#include "2d/physics/Physics2DLua.hpp"

namespace haylen::lua::modules {

void Physics2DModule::install(core::Engine&, lua_State* L) {
    physics2d::Physics2DLua::install(L);
}

} // namespace haylen::lua::modules
