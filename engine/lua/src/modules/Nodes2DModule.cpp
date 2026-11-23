#include "modules/Nodes2DModule.hpp"

#include <memory>

#include "2d/nodes/Nodes2DBinding.hpp"
#include "haylen/2d/nodes/Prefab.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"

namespace haylen::lua::modules {

void Nodes2DModule::install(haylen::core::Engine& engine, lua_State* L) {
    lua::nodes2d::Nodes2DBinding::install(L);

    // clang-format off
    AssetsModule& assets = engine.getPlugin<ScriptPlugin>().getModule<AssetsModule>();
    assets.registerPusher("prefab", [](lua_State* state, const std::shared_ptr<void>& asset) {
        lua::nodes2d::Nodes2DBinding::pushPrefab(state, std::static_pointer_cast<haylen::nodes2d::Prefab>(asset));
    });
    // clang-format on
}

} // namespace haylen::lua::modules
