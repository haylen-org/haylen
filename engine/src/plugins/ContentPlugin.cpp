#include "plugins/ContentPlugin.hpp"

#include "content/ContentLua.hpp"
#include "content/UpdateScreen.hpp"
#include "haylen/content/ContentManager.hpp"
#include "haylen/core/Engine.hpp"

namespace haylen::plugins {

ContentPlugin::ContentPlugin() = default;

ContentPlugin::~ContentPlugin() = default;

void ContentPlugin::start(core::Engine& engine) {
    engine.getContent().start();
}

void ContentPlugin::stop(core::Engine&) {
    screen.reset();
}

void ContentPlugin::installLua(core::Engine&, lua_State* L) {
    content::ContentLua::install(L);
}

void ContentPlugin::event(core::Engine&, const platform::Event& event) {
    if (screen) {
        screen->handleEvent(event);
    }
}

// The update screen shows while the app waits for required content that downloads, or after such a download failed, and never for the moment it takes to find the content on the device. The downloads advance every frame.
void ContentPlugin::update(core::Engine& engine, float) {
    content::ContentManager& manager = engine.getContent();
    manager.update();
    const bool waiting = manager.getProgress().totalBytes > 0 || manager.getStatus() == content::ContentManager::Status::Failed;
    if (!manager.isHoldingStart() || !waiting) {
        screen.reset();
        return;
    }
    if (!screen) {
        screen = std::make_unique<content::UpdateScreen>(engine);
    }
    screen->update();
}

void ContentPlugin::renderOverlay(core::Engine&) {
    if (screen) {
        screen->render();
    }
}

bool ContentPlugin::isCapturingBack() const {
    return screen != nullptr;
}

} // namespace haylen::plugins
