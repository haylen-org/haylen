#pragma once

#include <memory>

namespace haylen::core {

class Engine;
struct AppConfig;

// Entry point of an app. The runtime creates it before the window, lets it adjust the configuration read from `haylen.json`, and starts it once the engine is ready.
class Application {
  public:
    virtual ~Application() = default;

    // Defined by the executable, not by the engine. The `haylen` player and generated Lua apps return a `lua::Application`.
    [[nodiscard]] static std::unique_ptr<Application> create();

    virtual void configure(AppConfig& config);

    // Adds the plugins of the app with `Engine::addPlugin` when the engine is made, after the plugins of the engine, so they start after them, such as the plugins that run the Lua of an app.
    virtual void addPlugins(Engine& engine);
    virtual void start(Engine& engine) = 0;
    virtual void stop(Engine& engine);
};

} // namespace haylen::core
