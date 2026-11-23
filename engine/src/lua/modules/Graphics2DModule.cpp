#include "lua/modules/Graphics2DModule.hpp"

#include "2d/graphics/Graphics2DLua.hpp"
#include "2d/lighting/Lighting2DLua.hpp"
#include "graphics/GraphicsLua.hpp"

namespace haylen::lua::modules {

void Graphics2DModule::install(core::Engine&, lua_State* L) {
    graphics::GraphicsLua::install(L);
    graphics2d::Graphics2DLua::install(L);
    lighting2d::Lighting2DLua::install(L);
}

} // namespace haylen::lua::modules
