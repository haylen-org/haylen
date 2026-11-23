#include "modules/Graphics2DModule.hpp"

#include "2d/graphics/Graphics2DBinding.hpp"
#include "2d/lighting/Lighting2DBinding.hpp"
#include "graphics/GraphicsBinding.hpp"

namespace haylen::lua::modules {

void Graphics2DModule::install(haylen::core::Engine&, lua_State* L) {
    lua::graphics::GraphicsBinding::install(L);
    lua::graphics2d::Graphics2DBinding::install(L);
    lua::lighting2d::Lighting2DBinding::install(L);
}

} // namespace haylen::lua::modules
