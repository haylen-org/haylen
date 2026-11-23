#include "lua/modules/NetModule.hpp"

#include "net/NetLua.hpp"

namespace haylen::lua::modules {

void NetModule::install(core::Engine&, lua_State* L) {
    net::NetLua::install(L);
}

} // namespace haylen::lua::modules
