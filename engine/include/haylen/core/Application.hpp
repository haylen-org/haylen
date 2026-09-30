#pragma once

#include <memory>

namespace haylen::core {

class Engine;
struct AppConfig;

// Entry point of an app. The runtime creates it before the window, lets it adjust the configuration read from `app.json`, and starts it once the engine is ready.
class Application {
  public:
    virtual ~Application() = default;

    // Defined by the executable, not by the engine. The `haylen` player and generated Lua apps return a `lua::Application`.
    [[nodiscard]] static std::unique_ptr<Application> create();

    virtual void configure(AppConfig& config);
    virtual void start(Engine& engine) = 0;
    virtual void stop(Engine& engine);
};

} // namespace haylen::core
