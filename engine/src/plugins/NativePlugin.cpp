#include "plugins/NativePlugin.hpp"

#include "platform/native/NativeCallbacks.hpp"
#include "platform/native/NativeLua.hpp"

namespace haylen::plugins {

void NativePlugin::stop(core::Engine&) {
    callbacks.reset();
}

void NativePlugin::installLua(core::Engine&, lua_State* L) {
    callbacks = std::make_shared<platform::NativeCallbacks>(L);
    platform::NativeLua::install(L);
}

} // namespace haylen::plugins
