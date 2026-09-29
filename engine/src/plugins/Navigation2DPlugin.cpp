#include "plugins/Navigation2DPlugin.hpp"

#include "2d/navigation/Navigation2DLua.hpp"

namespace haylen::plugins {

void Navigation2DPlugin::installLua(core::Engine&, lua_State* L) {
    navigation2d::Navigation2DLua::install(L);
}

} // namespace haylen::plugins
