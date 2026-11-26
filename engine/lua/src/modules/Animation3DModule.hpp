#pragma once

#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs `haylen.animation3d`, which plays clips of 3D animation.
class Animation3DModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.animation3d";
    }
    void install(haylen::core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::lua::modules
