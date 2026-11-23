#include "lua/modules/Nodes2DModule.hpp"

#include <memory>

#include "2d/nodes/Nodes2DLua.hpp"
#include "haylen/2d/nodes/Prefab.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"

namespace haylen::lua::modules {

void Nodes2DModule::install(core::Engine& engine, lua_State* L) {
    nodes2d::Nodes2DLua::install(L);

    // clang-format off
    AssetsModule& assets = engine.getPlugin<ScriptPlugin>().getModule<AssetsModule>();
    assets.registerPusher("prefab", [](lua_State* state, const std::shared_ptr<void>& asset) {
        nodes2d::Nodes2DLua::pushPrefab(state, std::static_pointer_cast<nodes2d::Prefab>(asset));
    });
    // clang-format on
}

} // namespace haylen::lua::modules
