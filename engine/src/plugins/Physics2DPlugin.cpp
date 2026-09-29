#include "plugins/Physics2DPlugin.hpp"

#include "2d/physics/Physics2DLua.hpp"

namespace haylen::plugins {

void Physics2DPlugin::installLua(core::Engine&, lua_State* L) {
    physics2d::Physics2DLua::install(L);
}

} // namespace haylen::plugins
