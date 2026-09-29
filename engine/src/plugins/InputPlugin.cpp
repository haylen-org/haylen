#include "plugins/InputPlugin.hpp"

#include "input/InputLua.hpp"

namespace haylen::plugins {

void InputPlugin::installLua(core::Engine&, lua_State* L) {
    input::InputLua::install(L);
}

} // namespace haylen::plugins
