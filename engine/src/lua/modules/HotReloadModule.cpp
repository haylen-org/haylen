#include "lua/modules/HotReloadModule.hpp"

#include "haylen/core/Engine.hpp"
#include "lua/HotReloadLua.hpp"
#include "plugins/HotReloadPlugin.hpp"

namespace haylen::lua::modules {

void HotReloadModule::install(core::Engine& engine, lua_State* L) {
    HotReloadLua::install(L);
    engine.getPlugin<plugins::HotReloadPlugin>().installGraph(L);
}

} // namespace haylen::lua::modules
