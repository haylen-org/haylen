#include "modules/VideoModule.hpp"

#include "graphics/VideoBinding.hpp"

namespace haylen::lua::modules {

void VideoModule::install(haylen::core::Engine&, lua_State* L) {
    lua::graphics::VideoBinding::install(L);
}

} // namespace haylen::lua::modules
