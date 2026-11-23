#include "modules/TiledModule.hpp"

#include <memory>

#include "2d/tiled/TiledBinding.hpp"
#include "haylen/2d/tiled/AutoMapper.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/World.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"

namespace haylen::lua::modules {

void TiledModule::install(haylen::core::Engine& engine, lua_State* L) {
    lua::tiled::TiledBinding::install(L);

    // clang-format off
    AssetsModule& assets = engine.getPlugin<ScriptPlugin>().getModule<AssetsModule>();
    assets.registerPusher("tiled", [](lua_State* state, const std::shared_ptr<void>& asset) {
        lua::tiled::TiledBinding::pushMap(state, std::static_pointer_cast<haylen::tiled::Map>(asset));
    });
    assets.registerPusher("tiledRules", [](lua_State* state, const std::shared_ptr<void>& asset) {
        lua::tiled::TiledBinding::pushAutoMapper(state, std::static_pointer_cast<haylen::tiled::AutoMapper>(asset));
    });
    assets.registerPusher("tiledWorld", [](lua_State* state, const std::shared_ptr<void>& asset) {
        lua::tiled::TiledBinding::pushWorld(state, *std::static_pointer_cast<haylen::tiled::World>(asset));
    });
    // clang-format on
}

} // namespace haylen::lua::modules
