#include "plugins/NativePlugin.hpp"

#include "haylen/core/Engine.hpp"
#include "haylen/platform/NativeLibraries.hpp"
#include "platform/native/NativeCallbacks.hpp"
#include "platform/native/NativeLua.hpp"

namespace haylen::plugins {

void NativePlugin::stop(core::Engine&) {
    callbacks.reset();
}

void NativePlugin::installLua(core::Engine& engine, lua_State* L) {
    platform::NativeLibraries::addLinkedSymbols(engine.getScriptRuntime());
    callbacks = std::make_shared<platform::NativeCallbacks>(L);
    platform::NativeLua::install(L);
}

} // namespace haylen::plugins
