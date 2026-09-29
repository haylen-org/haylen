#include "plugins/Spatial2DPlugin.hpp"

#include "2d/spatial/Spatial2DLua.hpp"

namespace haylen::plugins {

void Spatial2DPlugin::installLua(core::Engine&, lua_State* L) {
    spatial2d::Spatial2DLua::install(L);
}

} // namespace haylen::plugins
