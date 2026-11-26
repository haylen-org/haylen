#pragma once

#include <memory>
#include <string_view>

#include "haylen/3d/nodes/ComponentRegistry3D.hpp"
#include "haylen/3d/nodes/NodeTree3D.hpp"
#include "haylen/plugins/LiveObjects.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Holds the registry of the component types of 3D nodes, where built-in types and the types of plugins register, and runs the trees that it follows: their components in the fixed steps, before the scenes update and after them, by the process mode of each tree, and then the update of each tree on the job system, so the world state of every node is current before anything draws.
class Nodes3DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "nodes3d";
    }
    void stop(core::Engine& engine) override;
    void fixedUpdate(core::Engine& engine, float stepSeconds) override;
    void update(core::Engine& engine, float deltaSeconds) override;
    void lateUpdate(core::Engine& engine, float deltaSeconds) override;

    [[nodiscard]] nodes3d::ComponentRegistry3D& getComponentTypes() noexcept {
        return types;
    }

    // Makes a tree that the plugin follows while something else holds it.
    [[nodiscard]] std::shared_ptr<nodes3d::NodeTree3D> createTree();

  private:
    void run(core::Engine& engine, nodes3d::ComponentType3D::Phase phase, float seconds);

    nodes3d::ComponentRegistry3D types;
    LiveObjects<nodes3d::NodeTree3D> trees;
};

} // namespace haylen::plugins
