#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "haylen/core/Connection.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::physics2d {
class World;
}

namespace haylen::plugins {

// Installs `haylen.physics2d`, the Lua side of the Box2D worlds, and draws the bodies, shapes, joints and contacts of every live world Lua made for the `physics` debug drawing.
class Physics2DPlugin final : public Plugin {
  public:
    static constexpr std::string_view kDrawing = "physics";

    // A world that the debug drawing shows until it is gone or its link ends, which the owner of the world ends when it ends, so the world of a scene that unloaded never draws again, even before the collector frees it. A blocked world skips the drawing.
    class TrackedWorld final : public core::Connection::Link {
      public:
        explicit TrackedWorld(const std::shared_ptr<physics2d::World>& tracked);

        void disconnect() override;
        [[nodiscard]] bool isConnected() const noexcept override;
        void setBlocked(bool value) override;
        [[nodiscard]] bool isBlocked() const noexcept override;

        // Returns the world while it lives and its link holds, or null.
        [[nodiscard]] std::shared_ptr<physics2d::World> lock() const;

      private:
        std::weak_ptr<physics2d::World> world;
        bool connected = true;
        bool blocked = false;
    };

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "physics2d";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;

    // Keeps a world for the debug drawing for as long as it lives and the returned link holds, which the owner of the world ends.
    [[nodiscard]] std::shared_ptr<TrackedWorld> track(const std::shared_ptr<physics2d::World>& world);

  private:
    // Worlds draw in world and render target canvases, whose units are the units of the worlds.
    void draw(graphics2d::Renderer& renderer);

    std::vector<std::shared_ptr<TrackedWorld>> worlds;
    core::Connection drawer;
};

} // namespace haylen::plugins
