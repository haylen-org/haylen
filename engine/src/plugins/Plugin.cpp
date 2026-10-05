#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

void Plugin::start(core::Engine&) {}

void Plugin::stop(core::Engine&) {}

void Plugin::installLua(core::Engine&, lua_State*) {}

void Plugin::event(core::Engine&, const platform::Event&) {}

void Plugin::beginFrame(core::Engine&, float) {}

void Plugin::fixedUpdate(core::Engine&, float) {}

void Plugin::update(core::Engine&, float) {}

void Plugin::render(core::Engine&) {}

void Plugin::renderUi(core::Engine&, const core::SceneView&) {}

void Plugin::renderOverlay(core::Engine&) {}

void Plugin::endFrame(core::Engine&) {}

bool Plugin::isCapturingBack() const {
    return false;
}

} // namespace haylen::plugins
