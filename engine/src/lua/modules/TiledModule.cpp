#include "lua/modules/TiledModule.hpp"

#include <memory>

#include "2d/tiled/TiledLua.hpp"
#include "haylen/2d/tiled/AutoMapper.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/World.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"

namespace haylen::lua::modules {

void TiledModule::install(core::Engine& engine, lua_State* L) {
    tiled::TiledLua::install(L);

    // clang-format off
    AssetsModule& assets = engine.getPlugin<ScriptPlugin>().getModule<AssetsModule>();
    assets.registerPusher("tiled", [](lua_State* state, const std::shared_ptr<void>& asset) {
        tiled::TiledLua::pushMap(state, std::static_pointer_cast<tiled::Map>(asset));
    });
    assets.registerPusher("tiledRules", [](lua_State* state, const std::shared_ptr<void>& asset) {
        tiled::TiledLua::pushAutoMapper(state, std::static_pointer_cast<tiled::AutoMapper>(asset));
    });
    assets.registerPusher("tiledWorld", [](lua_State* state, const std::shared_ptr<void>& asset) {
        tiled::TiledLua::pushWorld(state, *std::static_pointer_cast<tiled::World>(asset));
    });
    // clang-format on
}

} // namespace haylen::lua::modules
