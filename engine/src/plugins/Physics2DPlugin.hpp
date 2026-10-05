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

// Installs `haylen.physics2d`, the Lua side of the Box2D worlds, and draws the bodies, shapes, joints and contacts of every world Lua made for the `physics` debug drawing.
class Physics2DPlugin final : public Plugin {
  public:
    static constexpr std::string_view kDrawing = "physics";

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "physics2d";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;

    // Keeps a world for the debug drawing for as long as it lives.
    void track(const std::shared_ptr<physics2d::World>& world);

  private:
    // Worlds draw in world and render target canvases, whose units are the units of the worlds.
    void draw(graphics2d::Renderer& renderer);

    std::vector<std::weak_ptr<physics2d::World>> worlds;
    core::Connection drawer;
};

} // namespace haylen::plugins
