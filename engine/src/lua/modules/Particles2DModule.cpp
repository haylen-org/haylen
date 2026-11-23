#include "lua/modules/Particles2DModule.hpp"

#include <memory>

#include "2d/particles/Particles2DLua.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"

namespace haylen::lua::modules {

void Particles2DModule::install(core::Engine& engine, lua_State* L) {
    particles2d::Particles2DLua::install(L);

    // clang-format off
    AssetsModule& assets = engine.getPlugin<ScriptPlugin>().getModule<AssetsModule>();
    assets.registerPusher("particles", [](lua_State* state, const std::shared_ptr<void>& asset) {
        particles2d::Particles2DLua::pushEffect(state, std::static_pointer_cast<particles2d::Effect>(asset));
    });
    assets.registerPusher("imageShape", [](lua_State* state, const std::shared_ptr<void>& asset) {
        particles2d::Particles2DLua::pushImageShape(state, std::static_pointer_cast<particles2d::ImageShape>(asset));
    });
    // clang-format on
}

} // namespace haylen::lua::modules
