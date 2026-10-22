#include <any>
#include <cmath>
#include <memory>
#include <numbers>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/text/Font.hpp"

namespace embedding {

// An app of its own project that runs the whole engine: its scene shows the symbol of Haylen with a comet of dots that circles it, under a title.
class EmbeddingApp final : public haylen::core::Application {
  public:
    void start(haylen::core::Engine& engine) override {
        engine.getScenes().push(std::make_shared<OrbitScene>());
    }

  private:
    class OrbitScene final : public haylen::core::Scene {
      public:
        static constexpr int kDots = 18;
        static constexpr float kOrbit = 250.0F;

        void enter(haylen::core::Engine& engine, const std::any&) override {
            symbol = engine.getAssets().vectorImage("symbol.svg");
        }

        void update(haylen::core::Engine&, float deltaSeconds) override {
            time += deltaSeconds;
        }

        void renderUi(haylen::core::Engine& engine) override {
            using haylen::math::Color;
            using haylen::math::Vec2;
            haylen::graphics2d::Renderer& renderer = engine.getRenderer2D();
            haylen::text::Font& font = *engine.getDefaultFont();
            const haylen::math::Rect safe = engine.getViewport().getSafeRect();
            const Vec2 center = safe.getCenter() + Vec2{0.0F, 40.0F};
            renderer.beginScreen();

            renderer.drawText(font, "Haylen from a C++ project", safe.getMin() + Vec2{64.0F, 56.0F}, {.size = 56.0F});
            renderer.drawText(font, "A CMake project of its own adds the engine and draws with its renderer.", safe.getMin() + Vec2{64.0F, 132.0F}, {.size = 28.0F, .color = Color::fromHex(0x9AA3B8FFU)});

            // A soft glow sits behind the symbol, which floats a little above and below the middle, and the far half of the comet passes behind the symbol.
            renderer.drawShape({.kind = haylen::graphics2d::Shape::Kind::Ellipse, .bounds = haylen::math::Rect::fromCenter(center, {300.0F, 300.0F}), .color = Color::fromHex(0x00D4E92EU), .softness = 220.0F});
            drawComet(renderer, center, false);
            const float bob = std::sin(time * 1.6F) * 8.0F;
            renderer.drawVector(symbol, {.position = center + Vec2{0.0F, bob}, .size = {172.0F, 205.0F}});
            drawComet(renderer, center, true);
        }

      private:
        // Draws the dots of the comet on the near or the far half of its orbit: a head of full dots and a tail that shrinks and fades behind it.
        void drawComet(haylen::graphics2d::Renderer& renderer, haylen::math::Vec2 center, bool near) const {
            using haylen::math::Color;
            for (int index = 0; index < kDots; ++index) {
                const float behind = static_cast<float>(index) / static_cast<float>(kDots);
                const float angle = time * 1.1F - behind * std::numbers::pi_v<float> * 0.9F;
                if ((std::sin(angle) >= 0.0F) != near) {
                    continue;
                }
                const haylen::math::Vec2 point = center + haylen::math::Vec2{std::cos(angle), std::sin(angle) * 0.38F} * kOrbit;
                const Color color = Color::lerp(Color::fromHex(0x00D4E9FFU), Color::fromHex(0x3D7BFFFFU), behind).withAlpha(1.0F - behind * 0.9F);
                renderer.drawCircle(point, 14.0F * (1.0F - behind * 0.7F), color);
            }
        }

        haylen::graphics::VectorImage symbol;
        float time = 0.0F;
    };
};

} // namespace embedding

std::unique_ptr<haylen::core::Application> haylen::core::Application::create() {
    return std::make_unique<embedding::EmbeddingApp>();
}
