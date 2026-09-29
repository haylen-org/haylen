#include "plugins/Graphics2DPlugin.hpp"

#include "2d/graphics/Graphics2DLua.hpp"
#include "2d/lighting/Lighting2DLua.hpp"
#include "graphics/GraphicsLua.hpp"

namespace haylen::plugins {

void Graphics2DPlugin::installLua(core::Engine&, lua_State* L) {
    graphics::GraphicsLua::install(L);
    graphics2d::Graphics2DLua::install(L);
    lighting2d::Lighting2DLua::install(L);
}

} // namespace haylen::plugins
