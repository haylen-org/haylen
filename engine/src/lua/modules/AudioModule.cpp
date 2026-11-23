#include "lua/modules/AudioModule.hpp"

#include <memory>

#include "audio/AudioLua.hpp"
#include "haylen/audio/Sound.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"

namespace haylen::lua::modules {

void AudioModule::install(core::Engine& engine, lua_State* L) {
    audio::AudioLua::install(L);

    // clang-format off
    AssetsModule& assets = engine.getPlugin<ScriptPlugin>().getModule<AssetsModule>();
    assets.registerPusher("sound", [](lua_State* state, const std::shared_ptr<void>& asset) {
        lua::Stack::push(state, audio::Sound(std::static_pointer_cast<const audio::SoundData>(asset)));
    });
    // clang-format on
}

} // namespace haylen::lua::modules
