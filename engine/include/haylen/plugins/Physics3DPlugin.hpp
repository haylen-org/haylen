#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "haylen/3d/physics/BackendRegistry.hpp"
#include "haylen/3d/physics/CollisionCategories.hpp"
#include "haylen/3d/physics/Shape.hpp"
#include "haylen/3d/physics/World.hpp"
#include "haylen/3d/physics/WorldSettings.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/plugins/LiveObjects.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Holds the backends of 3D physics, with the built-in `jolt` registered as the selected one, and the settings that the `physics3d` section of `haylen.json` gives every world. A plugin adds its own backend through `getBackends` in its `start`, which runs after this one, and selects it there.
class Physics3DPlugin final : public Plugin {
  public:
    // The world of an owner, which ends with its owner, such as the scene that made it. Blocking it changes nothing, since the app decides when a world steps.
    class OwnedWorld final : public core::Connection::Link {
      public:
        explicit OwnedWorld(const std::shared_ptr<physics3d::World>& owned);

        void disconnect() override;
        [[nodiscard]] bool isConnected() const noexcept override;
        void setBlocked(bool value) override;
        [[nodiscard]] bool isBlocked() const noexcept override;

      private:
        std::weak_ptr<physics3d::World> world;
        bool blocked = false;
    };

    // Worlds step on at most this many threads unless their settings ask for more.
    static constexpr int kDefaultThreads = 4;

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "physics3d";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;

    // Whether a world has awake bodies, which move at its next step.
    [[nodiscard]] bool isAnimating() const override;

    [[nodiscard]] physics3d::BackendRegistry& getBackends() noexcept {
        return backends;
    }

    // The settings of `haylen.json` that every world starts from, with as many threads as the workers of the engine, up to `kDefaultThreads`.
    [[nodiscard]] const physics3d::WorldSettings& getDefaultSettings() const noexcept {
        return defaults;
    }

    // The names of the collision categories of `physics3d.categories` in `haylen.json`.
    [[nodiscard]] const physics3d::CollisionCategories& getCategories() const noexcept {
        return categories;
    }

    // Makes a world with the selected backend on the job system of the engine, which the plugin follows while it lives. Throws as `physics3d::World` does.
    [[nodiscard]] std::shared_ptr<physics3d::World> createWorld(const physics3d::WorldSettings& settings);

    // Makes a shape with the selected backend. Throws as `physics3d::Backend::createShape` does.
    [[nodiscard]] std::shared_ptr<const physics3d::Shape> createShape(const physics3d::Shape::Settings& settings) const;

    // Makes a shape with the selected backend on the task pool, which suits meshes, hulls and heightfields, and hands the shape or the sentence of its failure to the completion on the frame thread.
    using ShapeCompletion = std::function<void(core::JobSystem::Result<std::shared_ptr<const physics3d::Shape>>)>;
    void createShapeAsync(physics3d::Shape::Settings settings, ShapeCompletion completion) const;

  private:
    core::Engine* running = nullptr;
    physics3d::BackendRegistry backends;
    physics3d::WorldSettings defaults;
    physics3d::CollisionCategories categories;
    LiveObjects<physics3d::World> worlds;
};

} // namespace haylen::plugins
