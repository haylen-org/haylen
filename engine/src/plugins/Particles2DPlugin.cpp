#include "plugins/Particles2DPlugin.hpp"

#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include "2d/particles/Particles2DLua.hpp"
#include "haylen/2d/particles/Effect.hpp"
#include "haylen/2d/particles/ImageShape.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JsonValidator.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/plugins/AssetsPlugin.hpp"

namespace haylen::plugins {

struct Particles2DPlugin::DecodedEffect {
    particles2d::Effect effect;
    std::map<std::string, graphics::Image> images;
};

// The texture options a caller passes stay as given, so they win over the options of the effect file only where they name a key.
core::Json Particles2DPlugin::normalizeEffectOptions(const core::Json& options) {
    if (options.is_null()) {
        return core::Json::object();
    }
    core::JsonValidator::requireKnownKeys(options, {"filter", "wrap"}, "texture options");
    (void)assets::Manager::textureOptionsFromJson(options);
    return options;
}

// Image shapes read `source` as `[x, y, width, height]` and `alphaThreshold` as a number.
particles2d::ImageShape::Options Particles2DPlugin::shapeOptionsFromJson(const core::Json& options) {
    particles2d::ImageShape::Options result;
    if (options.is_null()) {
        return result;
    }
    core::JsonValidator::requireKnownKeys(options, {"source", "alphaThreshold"}, "image shape options");
    if (options.contains("source")) {
        const core::Json& source = options.at("source");
        if (!source.is_array() || source.size() != 4) {
            throw std::invalid_argument("The source of an image shape needs four numbers: x, y, width and height.");
        }
        result.source = {source[0].get<float>(), source[1].get<float>(), source[2].get<float>(), source[3].get<float>()};
    }
    if (options.contains("alphaThreshold")) {
        result.alphaThreshold = options.at("alphaThreshold").get<float>();
    }
    return result;
}

void Particles2DPlugin::start(core::Engine& engine) {
    // clang-format off
    engine.getAssets().registerType({
        .name = "particles",
        .extensions = {".particles"},
        .normalize = [](const core::Json& options) { return normalizeEffectOptions(options); },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            const io::Package& package = *request.package;
            auto decoded = std::make_shared<DecodedEffect>();
            decoded->effect = particles2d::Effect::parse(core::Json::parse(request.bytes.begin(), request.bytes.end()), request.path, [&package](const std::string& path) {
                return package.readAsset(path);
            });
            for (const std::string& path : decoded->effect.getTexturePaths()) {
                decoded->images.emplace(path, graphics::Image::decode(package.readAsset(path)));
            }
            return decoded;
        },
        .finalize = [&manager = engine.getAssets(), &device = engine.getGraphics()](std::shared_ptr<void> result, const assets::Manager::Request& request) -> std::shared_ptr<void> {
            auto& decoded = *std::static_pointer_cast<DecodedEffect>(result);
            decoded.effect.applyTextures([&](const std::string& path, const core::Json& fileOptions) {
                core::Json options = fileOptions;
                options.update(request.options);
                const auto resource = manager.share("texture", path, options, [&](const assets::Manager::Request& texture) {
                    return device.createTexture(decoded.images.at(path), assets::Manager::textureOptionsFromJson(texture.options)).getResource();
                });
                return graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(resource));
            });
            return std::make_shared<particles2d::Effect>(std::move(decoded.effect));
        },
    });
    engine.getAssets().registerType({
        .name = "imageShape",
        .extensions = {},
        .normalize = [](const core::Json& options) {
            (void)shapeOptionsFromJson(options);
            return options.is_null() ? core::Json::object() : options;
        },
        .decode = [](assets::Manager::Request& request) -> std::shared_ptr<void> {
            return std::make_shared<particles2d::ImageShape>(graphics::Image::decode(request.bytes), shapeOptionsFromJson(request.options));
        },
        .finalize = [](std::shared_ptr<void> result, const assets::Manager::Request&) { return result; },
    });
    engine.getPlugin<AssetsPlugin>().registerLuaPusher("particles", [](lua_State* L, const std::shared_ptr<void>& asset) {
        particles2d::Particles2DLua::pushEffect(L, std::static_pointer_cast<particles2d::Effect>(asset));
    });
    engine.getPlugin<AssetsPlugin>().registerLuaPusher("imageShape", [](lua_State* L, const std::shared_ptr<void>& asset) {
        particles2d::Particles2DLua::pushImageShape(L, std::static_pointer_cast<particles2d::ImageShape>(asset));
    });
    // clang-format on
}

void Particles2DPlugin::installLua(core::Engine&, lua_State* L) {
    particles2d::Particles2DLua::install(L);
}

} // namespace haylen::plugins
