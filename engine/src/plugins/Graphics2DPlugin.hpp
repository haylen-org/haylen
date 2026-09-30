#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Installs `haylen.graphics` with textures, render targets and fonts, `haylen.graphics2d` with the 2D renderer and camera, and `haylen.lighting2d`.
class Graphics2DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "graphics2d";
    }
    void installLua(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::plugins
