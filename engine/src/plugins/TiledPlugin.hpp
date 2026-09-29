#pragma once

#include "haylen/math/Color.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::graphics {
class Image;
}

namespace haylen::plugins {

// Registers the tiled asset type for .tmj maps and the tiledWorld type for .world files, and installs the haylen.tiled module.
class TiledPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "tiled";
    }
    void start(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;

  private:
    struct DecodedImage;
    struct DecodedMap;

    // Clears every pixel that matches the transparent color, the way Tiled shows images without an alpha channel.
    static void applyColorKey(graphics::Image& image, math::Color key);
};

} // namespace haylen::plugins
