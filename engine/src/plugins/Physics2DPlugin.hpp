#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Installs `haylen.physics2d`, the Lua side of the Box2D worlds.
class Physics2DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "physics2d";
    }
    void installLua(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::plugins
