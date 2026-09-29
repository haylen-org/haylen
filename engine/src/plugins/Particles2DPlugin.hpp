#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Registers the particle effect asset type for .particles files, whose texture shares the texture cache, and installs the haylen.particles2d module.
class Particles2DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "particles2d";
    }
    void start(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;

  private:
    struct DecodedEffect;
};

} // namespace haylen::plugins
