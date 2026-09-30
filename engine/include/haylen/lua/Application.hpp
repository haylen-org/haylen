#pragma once

#include "haylen/core/Application.hpp"

namespace haylen::lua {

// Runs `source/main.lua` from the app package, after loading the autoloads that `app.json` lists. Apps written in Lua use it as their application.
class Application final : public core::Application {
  public:
    void start(core::Engine& engine) override;

    // Stops the autoloads, the last one first, while every other system still runs.
    void stop(core::Engine& engine) override;
};

} // namespace haylen::lua
