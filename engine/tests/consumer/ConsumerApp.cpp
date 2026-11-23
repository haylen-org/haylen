#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/text/Font.hpp"

namespace consumer {

// An app of a project that adds the engine like another repository does, whose scene writes one line in the safe area. Launched with the argument `--quit`, as the run on the headless host is, it quits once it drew that line.
class ConsumerApp final : public haylen::core::Application {
  public:
    static constexpr std::string_view kQuitArgument = "--quit";

    void start(haylen::core::Engine& engine) override {
        const std::span<const std::string> arguments = engine.getLaunchArguments();
        const bool quits = std::ranges::find(arguments, kQuitArgument) != arguments.end();
        engine.getScenes().push(std::make_shared<GreetingScene>(quits));
    }

  private:
    class GreetingScene final : public haylen::core::Scene {
      public:
        explicit GreetingScene(bool value) : quits(value) {}

        void renderUi(haylen::core::Engine& engine) override {
            haylen::graphics2d::Renderer& renderer = engine.getRenderer2D();
            renderer.beginScreen();
            renderer.drawText(*engine.getDefaultFont(), "Haylen from another project", engine.getViewport().getSafeRect().getMin(), {.size = 48.0F});
            if (quits) {
                engine.quit();
            }
        }

      private:
        bool quits = false;
    };
};

} // namespace consumer

std::unique_ptr<haylen::core::Application> haylen::core::Application::create() {
    return std::make_unique<consumer::ConsumerApp>();
}
