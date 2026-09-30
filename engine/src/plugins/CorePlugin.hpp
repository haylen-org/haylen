#pragma once

#include "haylen/plugins/Plugin.hpp"
#include "lua/Autoloads.hpp"

namespace haylen::plugins {

// Installs the Lua modules of the engine core: `haylen`, `haylen.log`, `haylen.timer`, `haylen.window`, `haylen.viewport`, `haylen.scene`, `haylen.signal`, `haylen.events` and `haylen.tween`, with `haylen.math` and `haylen.ai`. It also runs the autoloads of the app, before the scenes update and after they render.
class CorePlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "core";
    }
    void stop(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
    void event(core::Engine& engine, const platform::Event& event) override;
    void fixedUpdate(core::Engine& engine, float stepSeconds) override;
    void update(core::Engine& engine, float deltaSeconds) override;
    void render(core::Engine& engine) override;
    void renderUi(core::Engine& engine) override;

    [[nodiscard]] lua::Autoloads& getAutoloads() noexcept {
        return autoloads;
    }

  private:
    lua::Autoloads autoloads;
};

} // namespace haylen::plugins
