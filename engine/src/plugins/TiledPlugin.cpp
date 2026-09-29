#include "plugins/TiledPlugin.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "2d/tiled/TiledLua.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/World.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JsonValidator.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/plugins/AssetsPlugin.hpp"

namespace haylen::plugins {

struct TiledPlugin::DecodedImage {
    graphics::Image image;
    bool keyed = false;
};

struct TiledPlugin::DecodedMap {
    tiled::Map map;
    std::map<std::string, DecodedImage> images;
};

void TiledPlugin::applyColorKey(graphics::Image& image, math::Color key) {
    const std::uint32_t target = key.withAlpha(1.0F).toRgba8();
    for (int y = 0; y < image.getHeight(); ++y) {
        for (int x = 0; x < image.getWidth(); ++x) {
            if (image.getPixel(x, y).withAlpha(1.0F).toRgba8() == target) {
                image.setPixel(x, y, math::Color::transparent());
            }
        }
    }
}

void TiledPlugin::start(core::Engine& engine) {
    // clang-format off
    engine.getAssets().registerType({
        .name = "tiled",
        .extensions = {".tmj"},
        .normalize = [&manager = engine.getAssets()](const core::Json& options) { return manager.normalizeOptions("texture", options); },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            const io::Package& package = *request.package;
            const auto reader = [&package](const std::string& path) { return core::Json::parse(package.readAssetText(path)); };
            auto decoded = std::make_shared<DecodedMap>();
            decoded->map = tiled::Map::parse(core::Json::parse(request.bytes.begin(), request.bytes.end()), request.path, reader);
            for (const tiled::Map::Image& entry : decoded->map.getImages()) {
                DecodedImage image{.image = graphics::Image::decode(package.readAsset(entry.path)), .keyed = entry.transparentColor.has_value()};
                if (entry.transparentColor) {
                    applyColorKey(image.image, *entry.transparentColor);
                }
                decoded->images.emplace(entry.path, std::move(image));
            }
            return decoded;
        },
        .finalize = [&manager = engine.getAssets(), &device = engine.getGraphics()](std::shared_ptr<void> result, const assets::Manager::Request& request) -> std::shared_ptr<void> {
            auto& decoded = *std::static_pointer_cast<DecodedMap>(result);
            const graphics::Texture::Options options = assets::Manager::textureOptionsFromJson(request.options);

            // Images without a color key share the texture cache, and keyed images stay private to the map because their pixels differ from the file.
            decoded.map.attachTextures([&](const std::string& path) {
                const DecodedImage& image = decoded.images.at(path);
                if (image.keyed) {
                    return device.createTexture(image.image, options);
                }
                return graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(manager.share("texture", path, request.options, [&](const assets::Manager::Request&) {
                    return device.createTexture(image.image, options).getResource();
                })));
            });
            return std::make_shared<tiled::Map>(std::move(decoded.map));
        },
    });
    engine.getAssets().registerType({
        .name = "tiledWorld",
        .extensions = {".world"},
        .normalize = [](const core::Json& options) {
            core::JsonValidator::requireKnownKeys(options, {}, "Tiled world options");
            return core::Json::object();
        },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            const std::vector<std::string> files = request.package->listAssets(io::Path::directory(request.path));
            return std::make_shared<tiled::World>(tiled::World::parse(core::Json::parse(request.bytes.begin(), request.bytes.end()), request.path, files));
        },
        .finalize = [](std::shared_ptr<void> decoded, const assets::Manager::Request&) { return decoded; },
    });

    AssetsPlugin& assetsPlugin = engine.getPlugin<AssetsPlugin>();
    assetsPlugin.registerLuaPusher("tiled", [](lua_State* L, const std::shared_ptr<void>& asset) {
        tiled::TiledLua::pushMap(L, std::static_pointer_cast<tiled::Map>(asset));
    });
    assetsPlugin.registerLuaPusher("tiledWorld", [](lua_State* L, const std::shared_ptr<void>& asset) {
        tiled::TiledLua::pushWorld(L, *std::static_pointer_cast<tiled::World>(asset));
    });
    // clang-format on
}

void TiledPlugin::installLua(core::Engine&, lua_State* L) {
    tiled::TiledLua::install(L);
}

} // namespace haylen::plugins
