#include "modules/HotReloadModule.hpp"

#include "development/HotReloadBinding.hpp"
#include "development/ModuleHotReload.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/plugins/HotReloadPlugin.hpp"

namespace haylen::lua::modules {

HotReloadModule::HotReloadModule() = default;

HotReloadModule::~HotReloadModule() = default;

void HotReloadModule::install(haylen::core::Engine& engine, lua_State* L) {
    HotReloadBinding::install(L);

    haylen::plugins::HotReloadPlugin& plugin = engine.getPlugin<haylen::plugins::HotReloadPlugin>();
    if (plugin.isActive()) {
        reload = std::make_unique<ModuleHotReload>(L);
        plugin.setScripts(reload.get());
    }
}

ModuleGraph* HotReloadModule::getGraph() noexcept {
    return reload ? &reload->getGraph() : nullptr;
}

void HotReloadModule::stop(haylen::core::Engine& engine) {
    engine.getPlugin<haylen::plugins::HotReloadPlugin>().setScripts(nullptr);
}

} // namespace haylen::lua::modules
