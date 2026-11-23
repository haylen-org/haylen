#pragma once

#include <memory>

#include "haylen/2d/nodes/NodeTree.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "plugins/LiveObjects.hpp"

namespace haylen::plugins {

// Registers the `prefab` asset type for `.prefab` files, which are checked on a worker thread. It follows the trees that the app creates, so an app that redraws on change draws while the particles of a node live.
class Nodes2DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "nodes2d";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;

    // Whether the particles of a node of a tree live, which the next update of the tree moves.
    [[nodiscard]] bool isAnimating() const override;

    void track(const std::shared_ptr<nodes2d::NodeTree>& tree);

  private:
    LiveObjects<nodes2d::NodeTree> trees;
};

} // namespace haylen::plugins
