#pragma once

#include <functional>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"

namespace haylen::test {

// Scene that runs a drawing function inside a real engine frame.
class DrawingScene final : public core::Scene {
  public:
    explicit DrawingScene(std::function<void(core::Engine&)> function) : draw(std::move(function)) {}

    void render(core::Engine& engine) override {
        draw(engine);
    }

  private:
    std::function<void(core::Engine&)> draw;
};

} // namespace haylen::test
