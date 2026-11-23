#include "modules/Animation2DModule.hpp"

#include <memory>

#include "2d/animation/Animation2DBinding.hpp"
#include "2d/animation/SkeletonBinding.hpp"
#include "haylen/2d/animation/SkeletonData.hpp"
#include "haylen/2d/animation/SpriteAtlas.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"

namespace haylen::lua::modules {

void Animation2DModule::install(haylen::core::Engine& engine, lua_State* L) {
    lua::animation2d::Animation2DBinding::install(L);

    // clang-format off
    AssetsModule& assets = engine.getPlugin<ScriptPlugin>().getModule<AssetsModule>();
    assets.registerPusher("atlas", [](lua_State* state, const std::shared_ptr<void>& asset) {
        lua::Stack::push(state, std::static_pointer_cast<haylen::animation2d::SpriteAtlas>(asset));
    });
    assets.registerPusher("skeleton", [](lua_State* state, const std::shared_ptr<void>& asset) {
        lua::animation2d::SkeletonBinding::pushData(state, std::static_pointer_cast<haylen::animation2d::SkeletonData>(asset));
    });
    // clang-format on
}

} // namespace haylen::lua::modules
