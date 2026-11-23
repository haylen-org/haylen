#include "modules/NetModule.hpp"

#include "net/NetBinding.hpp"

namespace haylen::lua::modules {

void NetModule::install(haylen::core::Engine&, lua_State* L) {
    lua::net::NetBinding::install(L);
}

} // namespace haylen::lua::modules
