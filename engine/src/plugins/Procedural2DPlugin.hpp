#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Installs haylen.procedural2d, the procedural generation of maps and the scattering of objects over them.
class Procedural2DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "procedural2d";
    }
    void installLua(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::plugins
