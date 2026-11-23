#pragma once

#include "haylen/core/Application.hpp"

namespace haylen::lua {

// Runs `source/main.lua` from the app package, after loading the autoloads that `haylen.json` lists. Apps written in Lua use it as their application, which adds the script plugin and the development tools of Lua after the plugins of the engine.
class Application final : public core::Application {
  public:
    void addPlugins(core::Engine& engine) override;
    void start(core::Engine& engine) override;

    // Stops the autoloads, the last one first, while every other system still runs.
    void stop(core::Engine& engine) override;
};

} // namespace haylen::lua
