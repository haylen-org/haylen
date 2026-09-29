#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Installs haylen.spatial2d, the spatial hash for neighbor queries.
class Spatial2DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "spatial2d";
    }
    void installLua(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::plugins
