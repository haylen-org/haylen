#include "plugins/Physics2DPlugin.hpp"

#include <limits>
#include <string>

#include "2d/physics/Physics2DLua.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/plugins/DebugPlugin.hpp"

namespace haylen::plugins {

void Physics2DPlugin::start(core::Engine& engine) {
    drawer = engine.getPlugin<DebugPlugin>().addDrawer(std::string(kDrawing), [this](graphics2d::Renderer& renderer) { draw(renderer); });
}

void Physics2DPlugin::stop(core::Engine&) {
    drawer.disconnect();
    worlds.clear();
}

void Physics2DPlugin::installLua(core::Engine&, lua_State* L) {
    physics2d::Physics2DLua::install(L);
}

void Physics2DPlugin::track(const std::shared_ptr<physics2d::World>& world) {
    std::erase_if(worlds, [](const std::weak_ptr<physics2d::World>& known) { return known.expired(); });
    worlds.push_back(world);
}

void Physics2DPlugin::draw(graphics2d::Renderer& renderer) {
    if (renderer.getCanvasKind() == graphics2d::Renderer::CanvasKind::Screen) {
        return;
    }
    for (const std::weak_ptr<physics2d::World>& known : worlds) {
        if (const std::shared_ptr<physics2d::World> world = known.lock()) {
            world->debugDraw(renderer, {.layer = std::numeric_limits<int>::max() / 2, .unshaded = true});
        }
    }
}

} // namespace haylen::plugins
