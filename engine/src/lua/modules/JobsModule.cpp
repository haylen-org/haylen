#include "lua/modules/JobsModule.hpp"

#include "lua/JobsLua.hpp"

namespace haylen::lua::modules {

void JobsModule::install(core::Engine&, lua_State* L) {
    JobsLua::install(L);
}

} // namespace haylen::lua::modules
