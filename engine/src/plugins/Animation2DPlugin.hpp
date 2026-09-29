#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Registers the atlas asset type for TexturePacker and Aseprite JSON, whose image shares the texture cache, and installs the haylen.animation2d module.
class Animation2DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "animation2d";
    }
    void start(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;

  private:
    struct DecodedAtlas;
};

} // namespace haylen::plugins
