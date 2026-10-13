#include "plugins/Animation2DPlugin.hpp"

#include <memory>
#include <string>
#include <vector>

#include "2d/animation/Animation2DLua.hpp"
#include "2d/animation/SkeletonLua.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/2d/animation/SkeletonData.hpp"
#include "haylen/2d/animation/SpriteAtlas.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/plugins/AssetsPlugin.hpp"

namespace haylen::plugins {

struct Animation2DPlugin::DecodedAtlas {
    nlohmann::ordered_json document;
    std::string imagePath;
    graphics::Image image;
};

struct Animation2DPlugin::DecodedSkeleton {
    core::Json document;
    std::string atlasPath;
    DecodedAtlas atlas;
};

void Animation2DPlugin::start(core::Engine& engine) {
    // clang-format off
    engine.getAssets().registerType({
        .name = "atlas",
        .extensions = {},
        .normalize = [&manager = engine.getAssets()](const core::Json& options) { return manager.normalizeOptions("texture", options); },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            auto decoded = std::make_shared<DecodedAtlas>();
            decoded->document = nlohmann::ordered_json::parse(request.bytes.begin(), request.bytes.end());
            decoded->imagePath = io::Path::join(io::Path::directory(request.path), animation2d::SpriteAtlas::imagePath(decoded->document));
            decoded->image = graphics::Image::decode(request.package->readAsset(decoded->imagePath));
            return decoded;
        },
        .finalize = [&manager = engine.getAssets(), &device = engine.getGraphics()](std::shared_ptr<void> result, const assets::Manager::Request& request) -> std::shared_ptr<void> {
            const auto& decoded = *std::static_pointer_cast<DecodedAtlas>(result);
            const auto resource = manager.share("texture", decoded.imagePath, request.options, [&](const assets::Manager::Request& texture) {
                return device.createTexture(decoded.image, assets::Manager::textureOptionsFromJson(texture.options)).getResource();
            });
            return std::make_shared<animation2d::SpriteAtlas>(animation2d::SpriteAtlas::parse(decoded.document, graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(resource))));
        },
    });
    engine.getPlugin<AssetsPlugin>().registerLuaPusher("atlas", [](lua_State* L, const std::shared_ptr<void>& asset) {
        lua::Stack::push(L, std::static_pointer_cast<animation2d::SpriteAtlas>(asset));
    });

    // A skeleton reads its atlas and image with it on the worker thread, and shares the atlas and the texture with direct loads.
    engine.getAssets().registerType({
        .name = "skeleton",
        .extensions = {".skeleton"},
        .normalize = [&manager = engine.getAssets()](const core::Json& options) { return manager.normalizeOptions("texture", options); },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            auto decoded = std::make_shared<DecodedSkeleton>();
            decoded->document = core::Json::parse(request.bytes.begin(), request.bytes.end());
            decoded->atlasPath = io::Path::join(io::Path::directory(request.path), animation2d::SkeletonData::atlasPath(decoded->document));
            const std::vector<std::uint8_t> atlasBytes = request.package->readAsset(decoded->atlasPath);
            decoded->atlas.document = nlohmann::ordered_json::parse(atlasBytes.begin(), atlasBytes.end());
            decoded->atlas.imagePath = io::Path::join(io::Path::directory(decoded->atlasPath), animation2d::SpriteAtlas::imagePath(decoded->atlas.document));
            decoded->atlas.image = graphics::Image::decode(request.package->readAsset(decoded->atlas.imagePath));
            return decoded;
        },
        .finalize = [&manager = engine.getAssets(), &device = engine.getGraphics()](std::shared_ptr<void> result, const assets::Manager::Request& request) -> std::shared_ptr<void> {
            const auto& decoded = *std::static_pointer_cast<DecodedSkeleton>(result);
            const auto atlas = std::static_pointer_cast<animation2d::SpriteAtlas>(manager.share("atlas", decoded.atlasPath, request.options, [&](const assets::Manager::Request&) -> std::shared_ptr<void> {
                const auto texture = manager.share("texture", decoded.atlas.imagePath, request.options, [&](const assets::Manager::Request& made) {
                    return device.createTexture(decoded.atlas.image, assets::Manager::textureOptionsFromJson(made.options)).getResource();
                });
                return std::make_shared<animation2d::SpriteAtlas>(animation2d::SpriteAtlas::parse(decoded.atlas.document, graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(texture))));
            }));
            return std::make_shared<animation2d::SkeletonData>(animation2d::SkeletonData::parse(decoded.document, atlas));
        },
    });
    engine.getPlugin<AssetsPlugin>().registerLuaPusher("skeleton", [](lua_State* L, const std::shared_ptr<void>& asset) {
        animation2d::SkeletonLua::pushData(L, std::static_pointer_cast<animation2d::SkeletonData>(asset));
    });
    // clang-format on
}

void Animation2DPlugin::installLua(core::Engine&, lua_State* L) {
    animation2d::Animation2DLua::install(L);
}

} // namespace haylen::plugins
