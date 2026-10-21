#pragma once

#include "haylen/2d/particles/ImageShape.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Registers the particle effect asset type for `.particles` files, whose textures load as textures of the cache before the effect is finalized, and the `imageShape` asset type that image shapes of emitters load as, and installs the `haylen.particles2d` module.
class Particles2DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "particles2d";
    }
    void start(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;

  private:
    [[nodiscard]] static core::Json normalizeEffectOptions(const core::Json& options);
    [[nodiscard]] static core::Json textureOptions(const core::Json& fileOptions, const core::Json& loadOptions);
    [[nodiscard]] static particles2d::ImageShape::Options shapeOptionsFromJson(const core::Json& options);
};

} // namespace haylen::plugins
