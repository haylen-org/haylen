#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Installs `haylen.navigation2d` with grid path finding and steering.
class Navigation2DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "navigation2d";
    }
    void installLua(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::plugins
