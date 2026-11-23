#include "lua/modules/HotReloadModule.hpp"

#include "development/ModuleHotReload.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/plugins/HotReloadPlugin.hpp"
#include "lua/HotReloadLua.hpp"

namespace haylen::lua::modules {

HotReloadModule::HotReloadModule() = default;

HotReloadModule::~HotReloadModule() = default;

void HotReloadModule::install(core::Engine& engine, lua_State* L) {
    HotReloadLua::install(L);

    plugins::HotReloadPlugin& plugin = engine.getPlugin<plugins::HotReloadPlugin>();
    if (plugin.isActive()) {
        reload = std::make_unique<ModuleHotReload>(L);
        plugin.setScripts(reload.get());
    }
}

ModuleGraph* HotReloadModule::getGraph() noexcept {
    return reload ? &reload->getGraph() : nullptr;
}

void HotReloadModule::stop(core::Engine& engine) {
    engine.getPlugin<plugins::HotReloadPlugin>().setScripts(nullptr);
}

} // namespace haylen::lua::modules
