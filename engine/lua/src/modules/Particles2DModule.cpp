#include "modules/Particles2DModule.hpp"

#include <memory>

#include "2d/particles/Particles2DBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"

namespace haylen::lua::modules {

void Particles2DModule::install(haylen::core::Engine& engine, lua_State* L) {
    lua::particles2d::Particles2DBinding::install(L);

    // clang-format off
    AssetsModule& assets = engine.getPlugin<ScriptPlugin>().getModule<AssetsModule>();
    assets.registerPusher("particles", [](lua_State* state, const std::shared_ptr<void>& asset) {
        lua::particles2d::Particles2DBinding::pushEffect(state, std::static_pointer_cast<haylen::particles2d::Effect>(asset));
    });
    assets.registerPusher("imageShape", [](lua_State* state, const std::shared_ptr<void>& asset) {
        lua::particles2d::Particles2DBinding::pushImageShape(state, std::static_pointer_cast<haylen::particles2d::ImageShape>(asset));
    });
    // clang-format on
}

} // namespace haylen::lua::modules
