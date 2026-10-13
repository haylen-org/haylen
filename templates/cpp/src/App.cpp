#include <any>
#include <format>
#include <memory>
#include <string>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Bridge.hpp"
#include "haylen/text/Font.hpp"

namespace app {

// The starter C++ app. Its scene shows the logo and the name of the app, calls "app.greet" through the platform bridge and shows the answer of the native code of the platform with the count of the "app.greeted" events that it sends.
class App final : public haylen::core::Application {
  public:
    void start(haylen::core::Engine& engine) override {
        registerDesktopHandlers(engine);
        engine.getScenes().push(std::make_shared<HomeScene>());
    }

  private:
    using Bridge = haylen::platform::Bridge;

    class HomeScene final : public haylen::core::Scene {
      public:
        void enter(haylen::core::Engine& engine, const std::any&) override {
            logo = engine.getAssets().texture("logo.png");

            // The listener ends with the greeting, and the call holds it weakly, so neither runs once the scene is gone.
            Bridge& bridge = engine.getPlatform();
            // clang-format off
            bridge.on("app.greeted", [state = greeting.get()](const Bridge::Payload& event) {
                state->greetings = event.json.value("count", 0);
                state->show();
            }, greeting);
            bridge.call("app.greet", {.json = {{"name", "C++"}}}, [target = std::weak_ptr(greeting)](Bridge::Result result) {
                if (const std::shared_ptr<Greeting> state = target.lock()) {
                    state->answer = result.ok ? result.value.json.value("message", std::string()) : std::format("The native code did not answer: {}", result.error.message);
                    state->show();
                }
            });
            // clang-format on
        }

        void renderUi(haylen::core::Engine& engine) override {
            haylen::graphics2d::Renderer& renderer = engine.getRenderer2D();
            haylen::text::Font& font = *engine.getDefaultFont();
            const haylen::math::Vec2 center = engine.getViewport().getSafeRect().getCenter();
            renderer.beginScreen();
            renderer.draw({.texture = logo, .position = center + haylen::math::Vec2{0.0F, -180.0F}, .size = {256.0F, 256.0F}});
            renderer.drawText(font, engine.getConfig().name, center + haylen::math::Vec2{0.0F, 20.0F}, {.size = 64.0F, .anchor = {0.5F, 0.0F}});
            renderer.drawText(font, greeting->text, center + haylen::math::Vec2{0.0F, 140.0F}, {.size = 32.0F, .color = kMuted, .anchor = {0.5F, 0.0F}});
        }

      private:
        struct Greeting {
            std::string answer = "Asking the native code of the platform.";
            int greetings = 0;
            std::string text = answer;

            void show() {
                text = greetings == 0 ? answer : std::format("{} It sent {} {}.", answer, greetings, greetings == 1 ? "event" : "events");
            }
        };

        static constexpr haylen::math::Color kMuted = haylen::math::Color::fromHex(0xA7B0BEFFU);

        haylen::graphics::Texture logo;
        std::shared_ptr<Greeting> greeting = std::make_shared<Greeting>();
    };

    // Windows and Linux have no handler table in the language of the platform, so the app answers "app.greet" there in C++. A handler of the engine answers before the native code, so the other platforms leave the method to their native handlers.
    static void registerDesktopHandlers(haylen::core::Engine& engine) {
        if (engine.getPlatformName() != "windows" && engine.getPlatformName() != "linux") {
            return;
        }
        Bridge& bridge = engine.getPlatform();
        // clang-format off
        bridge.registerHandler("app.greet", [&bridge, greetings = 0](const Bridge::Payload& params, const Bridge::Reply& reply) mutable {
            greetings += 1;
            reply({.ok = true, .value = {.json = {{"message", std::format("Hello, {}, from C++.", params.json.value("name", std::string()))}}}});
            bridge.emit("app.greeted", haylen::core::Json{{"count", greetings}}.dump());
        });
        // clang-format on
    }
};

} // namespace app

std::unique_ptr<haylen::core::Application> haylen::core::Application::create() {
    return std::make_unique<app::App>();
}
