#include "plugins/Animation2DPlugin.hpp"

#include <memory>
#include <string>

#include "2d/animation/Animation2DLua.hpp"
#include "graphics/TextureResource.hpp"
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
    // clang-format on
}

void Animation2DPlugin::installLua(core::Engine&, lua_State* L) {
    animation2d::Animation2DLua::install(L);
}

} // namespace haylen::plugins
