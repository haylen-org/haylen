#include "lua/modules/Procedural2DModule.hpp"

#include "2d/procedural/Procedural2DLua.hpp"

namespace haylen::lua::modules {

void Procedural2DModule::install(core::Engine&, lua_State* L) {
    procedural2d::Procedural2DLua::install(L);
}

} // namespace haylen::lua::modules
