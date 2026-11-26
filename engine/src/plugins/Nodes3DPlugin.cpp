#include "haylen/plugins/Nodes3DPlugin.hpp"

#include <memory>

#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/JobSystem.hpp"

namespace haylen::plugins {

void Nodes3DPlugin::stop(core::Engine&) {
    trees.clear();
}

void Nodes3DPlugin::fixedUpdate(core::Engine& engine, float stepSeconds) {
    run(engine, nodes3d::ComponentType3D::Phase::FixedUpdate, stepSeconds);
}

void Nodes3DPlugin::update(core::Engine& engine, float deltaSeconds) {
    run(engine, nodes3d::ComponentType3D::Phase::Update, deltaSeconds);
}

// The trees update after their late components, which are the last to move nodes in the frame.
void Nodes3DPlugin::lateUpdate(core::Engine& engine, float deltaSeconds) {
    run(engine, nodes3d::ComponentType3D::Phase::LateUpdate, deltaSeconds);
    core::JobSystem& jobs = engine.getJobs();
    trees.forEach([&jobs](nodes3d::NodeTree3D& tree) { tree.update(&jobs); });
}

std::shared_ptr<nodes3d::NodeTree3D> Nodes3DPlugin::createTree() {
    std::shared_ptr<nodes3d::NodeTree3D> tree = nodes3d::NodeTree3D::create();
    trees.add(tree);
    return tree;
}

void Nodes3DPlugin::run(core::Engine& engine, nodes3d::ComponentType3D::Phase phase, float seconds) {
    const core::FrameClock& clock = engine.getClock();
    // clang-format off
    trees.forEach([&clock, phase, seconds](nodes3d::NodeTree3D& tree) {
        if (clock.canProcess(tree.getProcessMode())) {
            tree.getComponents().run(phase, seconds);
        }
    });
    // clang-format on
}

} // namespace haylen::plugins
