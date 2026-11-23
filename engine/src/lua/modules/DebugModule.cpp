#include "lua/modules/DebugModule.hpp"

#include "debug/DebugLua.hpp"

namespace haylen::lua::modules {

void DebugModule::install(core::Engine&, lua_State* L) {
    debug::DebugLua::install(L);
}

} // namespace haylen::lua::modules
