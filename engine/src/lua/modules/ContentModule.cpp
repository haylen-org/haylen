#include "lua/modules/ContentModule.hpp"

#include "content/ContentLua.hpp"

namespace haylen::lua::modules {

void ContentModule::install(core::Engine&, lua_State* L) {
    content::ContentLua::install(L);
}

} // namespace haylen::lua::modules
