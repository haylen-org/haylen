#pragma once

#include <memory>

#include "haylen/3d/navigation/NavMesh.hpp"
#include "haylen/plugins/LiveObjects.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Registers the `navmesh` asset type for `.navmesh` files, which are checked and assembled on a worker thread. It follows the navigation meshes that the app holds and starts the rebuilds of their tiles at the start of every frame on the task pool, so an app that redraws on change draws while a tile waits.
class Navigation3DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "navigation3d";
    }
    void start(core::Engine& engine) override;
    void beginFrame(core::Engine& engine, float deltaSeconds) override;
    void stop(core::Engine& engine) override;

    // Whether the tiles of a mesh wait for their rebuild.
    [[nodiscard]] bool isAnimating() const override;

    void track(const std::shared_ptr<navigation3d::NavMesh>& mesh);

  private:
    LiveObjects<navigation3d::NavMesh> meshes;
};

} // namespace haylen::plugins
