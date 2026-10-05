#include <memory>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/text/Font.hpp"

namespace consumer {

// An app of a project that adds the engine like another repository does, whose scene writes one line in the safe area.
class ConsumerApp final : public haylen::core::Application {
  public:
    void start(haylen::core::Engine& engine) override {
        engine.getScenes().push(std::make_shared<GreetingScene>());
    }

  private:
    class GreetingScene final : public haylen::core::Scene {
      public:
        void renderUi(haylen::core::Engine& engine) override {
            haylen::graphics2d::Renderer& renderer = engine.getRenderer2D();
            renderer.beginScreen();
            renderer.drawText(*engine.getDefaultFont(), "Haylen from another project", engine.getViewport().getSafeRect().getMin(), {.size = 48.0F});
        }
    };
};

} // namespace consumer

std::unique_ptr<haylen::core::Application> haylen::core::Application::create() {
    return std::make_unique<consumer::ConsumerApp>();
}
