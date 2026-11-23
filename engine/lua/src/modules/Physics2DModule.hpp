#pragma once

#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs `haylen.physics2d`, the worlds of Box2D with their bodies, joints and helpers.
class Physics2DModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.physics2d";
    }
    void install(haylen::core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::lua::modules
