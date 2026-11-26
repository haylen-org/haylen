#pragma once

#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs `haylen.physics3d`, the 3D physics worlds in meters that the selected backend simulates.
class Physics3DModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.physics3d";
    }
    void install(haylen::core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::lua::modules
