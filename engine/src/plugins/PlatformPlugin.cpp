#include "plugins/PlatformPlugin.hpp"

#include "haylen/core/Engine.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/DialogRelay.hpp"
#include "platform/DialogsLua.hpp"
#include "platform/PlatformLua.hpp"
#include "platform/SystemLua.hpp"

namespace haylen::plugins {

void PlatformPlugin::start(core::Engine& engine) {
    platform::BridgeRelay::attach(engine.getPlatform());
    platform::DialogRelay::attach(engine.getDialogs());
}

void PlatformPlugin::stop(core::Engine& engine) {
    platform::DialogRelay::detach(engine.getDialogs());
    platform::BridgeRelay::detach(engine.getPlatform());
}

void PlatformPlugin::installLua(core::Engine&, lua_State* L) {
    platform::PlatformLua::install(L);
    platform::SystemLua::install(L);
    platform::DialogsLua::install(L);
}

} // namespace haylen::plugins
