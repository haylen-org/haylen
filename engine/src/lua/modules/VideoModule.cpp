#include "lua/modules/VideoModule.hpp"

#include "graphics/VideoLua.hpp"

namespace haylen::lua::modules {

void VideoModule::install(core::Engine&, lua_State* L) {
    graphics::VideoLua::install(L);
}

} // namespace haylen::lua::modules
