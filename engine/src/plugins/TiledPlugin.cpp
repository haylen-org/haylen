#include "plugins/TiledPlugin.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "2d/tiled/TiledLua.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/2d/tiled/AutoMapper.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/PropertyTypes.hpp"
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

struct TiledPlugin::DecodedRules {
    std::shared_ptr<tiled::AutoMapper> mapper;
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

// The images of the tilesets of special automapping tiles never draw, so they stay unread, since the tileset Tiled adds for them names an image of the editor.
void TiledPlugin::decodeImages(const tiled::Map& map, const io::Package& package, const graphics::Texture::Options& options, std::map<std::string, DecodedImage>& images) {
    std::set<std::string> skipped;
    for (const tiled::Map::TilesetReference& reference : map.tilesets) {
        const tiled::Tileset& tileset = *reference.tileset;
        // clang-format off
        const bool special = tileset.tileCount > 0 && std::ranges::all_of(std::views::iota(0, tileset.tileCount), [&tileset](int id) {
            const tiled::Tile* tile = tileset.findTile(static_cast<std::uint32_t>(id));
            return tile != nullptr && tile->properties.has("MatchType");
        });
        // clang-format on
        if (special) {
            skipped.insert(tileset.image);
            for (const auto& [id, tile] : tileset.tiles) {
                skipped.insert(tile.image);
            }
        }
    }
    for (const tiled::Map::Image& entry : map.getImages()) {
        if (skipped.contains(entry.path) || images.contains(entry.path)) {
            continue;
        }
        DecodedImage image{.image = graphics::Image::decode(package.readAsset(entry.path)), .keyed = entry.transparentColor.has_value()};
        if (entry.transparentColor) {
            applyColorKey(image.image, *entry.transparentColor);
        }
        if (options.filter == graphics::Texture::Filter::Linear) {
            image.image.bleed();
        }
        if (options.mipmaps) {
            image.image.buildMipmaps();
        }
        images.emplace(entry.path, std::move(image));
    }
}

// Images without a color key share the texture cache, and keyed images stay private to the map because their pixels differ from the file. The skipped images of special tiles have no texture.
void TiledPlugin::attachImages(tiled::Map& map, const std::map<std::string, DecodedImage>& images, assets::Manager& manager, graphics::Device& device, const core::Json& textureJson) {
    const graphics::Texture::Options options = assets::Manager::textureOptionsFromJson(textureJson);
    // clang-format off
    map.attachTextures([&](const std::string& path) {
        const auto found = images.find(path);
        if (found == images.end()) {
            return graphics::Texture();
        }
        if (found->second.keyed) {
            return device.createTexture(found->second.image, options);
        }
        return graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(manager.share("texture", path, textureJson, [&](const assets::Manager::Request&) {
            return device.createTexture(found->second.image, options).getResource();
        })));
    });
    // clang-format on
}

core::Json TiledPlugin::getTextureOptions(const core::Json& options) {
    core::Json texture = options;
    texture.erase("project");
    return texture;
}

void TiledPlugin::start(core::Engine& engine) {
    // clang-format off
    engine.getAssets().registerType({
        .name = "tiled",
        .extensions = {".tmj"},
        .normalize = [&manager = engine.getAssets()](const core::Json& options) {
            core::Json normalized = manager.normalizeOptions("texture", getTextureOptions(options));
            if (options.contains("project")) {
                const core::Json& project = options.at("project");
                if (!project.is_string() || project.get<std::string>().empty()) {
                    throw std::invalid_argument("The \"project\" of a Tiled map is the path of its \".tiled-project\" file.");
                }
                normalized["project"] = io::Path::normalize(project.get<std::string>());
            }
            return normalized;
        },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            const io::Package& package = *request.package;
            const auto reader = [&package](const std::string& path) { return core::Json::parse(package.readAssetText(path)); };
            auto decoded = std::make_shared<DecodedMap>();
            const graphics::Texture::Options options = assets::Manager::textureOptionsFromJson(getTextureOptions(request.options));
            decoded->map = tiled::Map::parse(core::Json::parse(request.bytes.begin(), request.bytes.end()), request.path, reader);
            if (request.options.contains("project")) {
                decoded->map.applyPropertyTypes(tiled::PropertyTypes::parse(reader(request.options.at("project").get<std::string>())));
            }
            decodeImages(decoded->map, package, options, decoded->images);
            return decoded;
        },
        .finalize = [&manager = engine.getAssets(), &device = engine.getGraphics()](std::shared_ptr<void> result, const assets::Manager::Request& request) -> std::shared_ptr<void> {
            auto& decoded = *std::static_pointer_cast<DecodedMap>(result);
            attachImages(decoded.map, decoded.images, manager, device, getTextureOptions(request.options));
            return std::make_shared<tiled::Map>(std::move(decoded.map));
        },
    });
    engine.getAssets().registerType({
        .name = "tiledRules",
        .extensions = {},
        .normalize = [&manager = engine.getAssets()](const core::Json& options) { return manager.normalizeOptions("texture", options); },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            const io::Package& package = *request.package;
            const auto readText = [&package](const std::string& path) { return package.readAssetText(path); };
            const auto readJson = [&package](const std::string& path) { return core::Json::parse(package.readAssetText(path)); };
            auto decoded = std::make_shared<DecodedRules>();
            decoded->mapper = std::make_shared<tiled::AutoMapper>(tiled::AutoMapper::load(request.path, readText, readJson));
            const graphics::Texture::Options options = assets::Manager::textureOptionsFromJson(request.options);
            for (const tiled::AutoMapper::RuleMap& ruleMap : decoded->mapper->getRuleMaps()) {
                decodeImages(*ruleMap.map, package, options, decoded->images);
            }
            return decoded;
        },
        .finalize = [&manager = engine.getAssets(), &device = engine.getGraphics()](std::shared_ptr<void> result, const assets::Manager::Request& request) -> std::shared_ptr<void> {
            auto& decoded = *std::static_pointer_cast<DecodedRules>(result);
            for (const tiled::AutoMapper::RuleMap& ruleMap : decoded.mapper->getRuleMaps()) {
                attachImages(*ruleMap.map, decoded.images, manager, device, request.options);
            }
            return decoded.mapper;
        },
    });
    engine.getAssets().registerType({
        .name = "tiledWorld",
        .extensions = {".world"},
        .normalize = [](const core::Json& options) {
            core::JsonValidator::requireKnownOptions(options, {}, "Tiled world options");
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
    assetsPlugin.registerLuaPusher("tiledRules", [](lua_State* L, const std::shared_ptr<void>& asset) {
        tiled::TiledLua::pushAutoMapper(L, std::static_pointer_cast<tiled::AutoMapper>(asset));
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
