#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::test {

// Renders one engine frame with a drawing function and returns the stats of the 2D renderer.
class FrameRenderer final {
  public:
    static graphics2d::Renderer::Stats renderOnce(EngineFixture& fixture, std::function<void(core::Engine&)> draw) {
        fixture.engine().getScenes().replace(std::make_shared<DrawingScene>(std::move(draw)));
        fixture.frames(1);
        return fixture.engine().getRenderer2D().getStats();
    }
};

} // namespace haylen::test
