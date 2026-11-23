#include "lua/modules/InputModule.hpp"

#include "input/InputLua.hpp"

namespace haylen::lua::modules {

void InputModule::install(core::Engine&, lua_State* L) {
    input::InputLua::install(L);
}

} // namespace haylen::lua::modules
