#include "haylen/plugins/Physics3DPlugin.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "3d/physics/jolt/Backend.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"

namespace haylen::plugins {

void Physics3DPlugin::start(core::Engine& engine) {
    running = &engine;
    const int workers = static_cast<int>(engine.getJobs().getWorkerCount());
    defaults.threads = std::clamp(workers, 1, kDefaultThreads);
    defaults.readSection(engine.getConfig().physics3d);
    categories.readSection(engine.getConfig().physics3d);
    backends.add(std::make_unique<physics3d::jolt::Backend>());
}

void Physics3DPlugin::stop(core::Engine&) {
    worlds.clear();
    running = nullptr;
}

std::shared_ptr<physics3d::World> Physics3DPlugin::createWorld(const physics3d::WorldSettings& settings) {
    const std::shared_ptr<physics3d::World> world = std::make_shared<physics3d::World>(settings, backends.getSelected(), &running->getJobs());
    worlds.add(world);
    return world;
}

std::shared_ptr<const physics3d::Shape> Physics3DPlugin::createShape(const physics3d::Shape::Settings& settings) const {
    return backends.getSelected().createShape(settings);
}

// The pools of the engine stop before its plugins, so the backend outlives every shape that builds on them.
void Physics3DPlugin::createShapeAsync(physics3d::Shape::Settings settings, ShapeCompletion completion) const {
    const physics3d::Backend& backend = backends.getSelected();
    running->getJobs().run([&backend, settings = std::move(settings)] { return backend.createShape(settings); }, std::move(completion));
}

bool Physics3DPlugin::isAnimating() const {
    return worlds.any([](const physics3d::World& world) { return !world.isDestroyed() && world.getStats().activeBodies > 0; });
}

Physics3DPlugin::OwnedWorld::OwnedWorld(const std::shared_ptr<physics3d::World>& owned) : world(owned) {}

void Physics3DPlugin::OwnedWorld::disconnect() {
    if (const std::shared_ptr<physics3d::World> owned = world.lock()) {
        owned->destroy();
    }

    world.reset();
}

bool Physics3DPlugin::OwnedWorld::isConnected() const noexcept {
    const std::shared_ptr<physics3d::World> owned = world.lock();
    return owned != nullptr && !owned->isDestroyed();
}

void Physics3DPlugin::OwnedWorld::setBlocked(bool value) {
    blocked = value;
}

bool Physics3DPlugin::OwnedWorld::isBlocked() const noexcept {
    return blocked;
}

} // namespace haylen::plugins
