#include "lua/modules/NativeModule.hpp"

#include <memory>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/platform/NativeLibraries.hpp"
#include "platform/native/NativeCallbacks.hpp"
#include "platform/native/NativeLua.hpp"

namespace haylen::lua::modules {

void NativeModule::install(core::Engine& engine, lua_State* L) {
    platform::NativeLibraries::addLinkedSymbols(engine.getPlugin<ScriptPlugin>().getRuntime());
    callbacks = std::make_shared<platform::NativeCallbacks>(L);
    platform::NativeLua::install(L);
}

void NativeModule::stop(core::Engine&) {
    callbacks.reset();
}

} // namespace haylen::lua::modules
