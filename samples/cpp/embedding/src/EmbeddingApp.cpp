#include <cmath>
#include <memory>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/text/Font.hpp"

namespace embedding {

// An app of its own project that runs the whole engine: its scene draws a square that circles the middle of the safe area.
class EmbeddingApp final : public haylen::core::Application {
  public:
    void start(haylen::core::Engine& engine) override {
        engine.getScenes().push(std::make_shared<OrbitScene>());
    }

  private:
    class OrbitScene final : public haylen::core::Scene {
      public:
        void update(haylen::core::Engine&, float deltaSeconds) override {
            angle += deltaSeconds;
        }

        void renderUi(haylen::core::Engine& engine) override {
            haylen::graphics2d::Renderer& renderer = engine.getRenderer2D();
            const haylen::math::Rect safe = engine.getViewport().getSafeRect();
            const haylen::math::Vec2 center = safe.getCenter() + haylen::math::Vec2{std::cos(angle), std::sin(angle)} * 200.0F;
            renderer.beginScreen();
            renderer.drawRect(haylen::math::Rect::fromCenter(center, {96.0F, 96.0F}), haylen::math::Color::fromHex(0xF2B84BFFU));
            renderer.drawText(*engine.getDefaultFont(), "Haylen from a C++ project", safe.getMin() + haylen::math::Vec2{48.0F, 48.0F}, {.size = 48.0F});
        }

      private:
        float angle = 0.0F;
    };
};

} // namespace embedding

std::unique_ptr<haylen::core::Application> haylen::core::Application::create() {
    return std::make_unique<embedding::EmbeddingApp>();
}
