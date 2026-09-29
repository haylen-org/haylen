#include "plugins/Particles2DPlugin.hpp"

#include <memory>

#include "2d/particles/Particles2DLua.hpp"
#include "haylen/2d/particles/Effect.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/plugins/AssetsPlugin.hpp"

namespace haylen::plugins {

struct Particles2DPlugin::DecodedEffect {
    particles2d::Effect effect;
    graphics::Image image;
};

void Particles2DPlugin::start(core::Engine& engine) {
    // clang-format off
    engine.getAssets().registerType({
        .name = "particles",
        .extensions = {".particles"},
        .normalize = [&manager = engine.getAssets()](const core::Json& options) { return manager.normalizeOptions("texture", options); },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            auto decoded = std::make_shared<DecodedEffect>();
            decoded->effect = particles2d::Effect::parse(core::Json::parse(request.bytes.begin(), request.bytes.end()), request.path);
            decoded->image = graphics::Image::decode(request.package->readAsset(decoded->effect.texturePath));
            return decoded;
        },
        .finalize = [&manager = engine.getAssets(), &device = engine.getGraphics()](std::shared_ptr<void> result, const assets::Manager::Request& request) -> std::shared_ptr<void> {
            auto& decoded = *std::static_pointer_cast<DecodedEffect>(result);
            const auto resource = manager.share("texture", decoded.effect.texturePath, request.options, [&](const assets::Manager::Request& texture) {
                return device.createTexture(decoded.image, assets::Manager::textureOptionsFromJson(texture.options)).getResource();
            });
            decoded.effect.config.texture = graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(resource));
            return std::make_shared<particles2d::Effect>(std::move(decoded.effect));
        },
    });
    engine.getPlugin<AssetsPlugin>().registerLuaPusher("particles", [](lua_State* L, const std::shared_ptr<void>& asset) {
        particles2d::Particles2DLua::pushEffect(L, std::static_pointer_cast<particles2d::Effect>(asset));
    });
    // clang-format on
}

void Particles2DPlugin::installLua(core::Engine&, lua_State* L) {
    particles2d::Particles2DLua::install(L);
}

} // namespace haylen::plugins
