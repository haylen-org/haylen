#pragma once

#include <map>
#include <string>

#include "haylen/core/Json.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::assets {
class Manager;
}

namespace haylen::graphics {
class Device;
class Image;
} // namespace haylen::graphics

namespace haylen::io {
class Package;
}

namespace haylen::tiled {
class Map;
}

namespace haylen::plugins {

// Registers the `tiled` asset type for `.tmj` maps, the `tiledWorld` type for `.world` files and the `tiledRules` type for automapping rules, and installs the `haylen.tiled` module.
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
    struct DecodedRules;

    // Clears every pixel that matches the transparent color, the way Tiled shows images without an alpha channel.
    static void applyColorKey(graphics::Image& image, math::Color key);

    // Decodes the images of the tilesets and image layers of a map on a worker thread, and hands the map its textures on the frame thread.
    static void decodeImages(const tiled::Map& map, const io::Package& package, const graphics::Texture::Options& options, std::map<std::string, DecodedImage>& images);
    static void attachImages(tiled::Map& map, const std::map<std::string, DecodedImage>& images, assets::Manager& manager, graphics::Device& device, const core::Json& textureJson);

    // Returns the options of a map without its `project`, the options its images load with.
    [[nodiscard]] static core::Json getTextureOptions(const core::Json& options);
};

} // namespace haylen::plugins
