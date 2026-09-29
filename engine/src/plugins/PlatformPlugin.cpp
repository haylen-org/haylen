#include "plugins/PlatformPlugin.hpp"

#include "haylen/core/Engine.hpp"
#include "haylen/core/Version.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/platform/Bridge.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/PlatformLua.hpp"

namespace haylen::plugins {

void PlatformPlugin::start(core::Engine& engine) {
    platform::BridgeRelay::attach(engine.getPlatform());
    // clang-format off
    engine.getPlatform().registerHandler("engine.info", [&engine](const core::Json&, platform::Bridge::Reply reply) {
        reply({.ok = true, .value = {{"engine", "Haylen"}, {"version", core::Version::kString}, {"platform", engine.getPlatformName()}, {"backend", engine.getGraphics().getBackendName()}}});
    });
    engine.getPlatform().registerHandler("app.version", [&engine](const core::Json&, platform::Bridge::Reply reply) {
        reply({.ok = true, .value = engine.getConfig().version});
    });
    // clang-format on
}

void PlatformPlugin::stop(core::Engine& engine) {
    platform::BridgeRelay::detach(engine.getPlatform());
}

void PlatformPlugin::installLua(core::Engine&, lua_State* L) {
    platform::PlatformLua::install(L);
}

} // namespace haylen::plugins
