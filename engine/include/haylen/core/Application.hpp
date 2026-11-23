#pragma once

#include <memory>
#include <span>
#include <string_view>

namespace haylen::core {

class Engine;
struct AppConfig;

// Entry point of an app. The runtime creates it before it reads `haylen.json`, which may hold the sections that the application reads itself, lets it adjust the configuration, creates the window and starts it once the engine is ready.
class Application {
  public:
    virtual ~Application() = default;

    // Defined by the executable, not by the engine. The `haylen` player and generated Lua apps return a `lua::Application`.
    [[nodiscard]] static std::unique_ptr<Application> create();

    // The sections of `haylen.json` that the application reads itself, such as the `lua` section of a Lua app, which `AppConfig::sections` keeps for `configure`. The engine refuses every other key that it does not read.
    [[nodiscard]] virtual std::span<const std::string_view> getSections() const;

    // Adjusts the configuration read from `haylen.json` and validates the sections that the application reads, whose errors fail the app with their reason before the engine starts.
    virtual void configure(AppConfig& config);

    // Adds the plugins of the app with `Engine::addPlugin` when the engine is made, after the plugins of the engine, so they start after them, such as the plugins that run the Lua of an app.
    virtual void addPlugins(Engine& engine);
    virtual void start(Engine& engine) = 0;
    virtual void stop(Engine& engine);
};

} // namespace haylen::core
