#include "haylen/plugins/Particles2DPlugin.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/particles/Effect.hpp"
#include "haylen/2d/particles/ImageShape.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/JsonValidator.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/Package.hpp"

namespace haylen::plugins {

// The texture options a caller passes stay as given, so they win over the options of the effect file only where they name a key.
core::Json Particles2DPlugin::normalizeEffectOptions(const core::Json& options) {
    if (options.is_null()) {
        return core::Json::object();
    }

    (void)assets::Manager::textureOptionsFromJson(options);
    return options;
}

// The options of the texture of an image of an effect: those the file gives, with the options of the load on top.
core::Json Particles2DPlugin::textureOptions(const core::Json& fileOptions, const core::Json& loadOptions) {
    core::Json options = fileOptions;
    options.update(loadOptions);
    return options;
}

// Image shapes read `source` as `[x, y, width, height]` and `alphaThreshold` as a number.
particles2d::ImageShape::Options Particles2DPlugin::shapeOptionsFromJson(const core::Json& options) {
    particles2d::ImageShape::Options result;
    if (options.is_null()) {
        return result;
    }

    core::JsonValidator::requireKnownOptions(options, {"source", "alphaThreshold"}, "image shape options");

    if (options.contains("source")) {
        const core::Json& source = options.at("source");
        if (!source.is_array() || source.size() != 4) {
            throw std::invalid_argument("The source of an image shape needs four numbers: x, y, width and height.");
        }

        result.source = {core::JsonNumber::toFloat(source[0]), core::JsonNumber::toFloat(source[1]), core::JsonNumber::toFloat(source[2]), core::JsonNumber::toFloat(source[3])};
    }

    if (options.contains("alphaThreshold")) {
        result.alphaThreshold = core::JsonNumber::toFloat(options.at("alphaThreshold"));
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
            return std::make_shared<particles2d::Effect>(particles2d::Effect::parse(core::Json::parse(request.bytes.begin(), request.bytes.end()), request.path, [&package](const std::string& path) {
                return package.readAsset(path);
            }));
        },
        .finalize = [&manager = engine.getAssets()](std::shared_ptr<void> result, const assets::Manager::Request& request) -> std::shared_ptr<void> {
            auto effect = std::static_pointer_cast<particles2d::Effect>(result);
            effect->applyTextures([&](const std::string& path, const core::Json& fileOptions) {
                return graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(manager.load("texture", path, textureOptions(fileOptions, request.options))));
            });
            return effect;
        },
        .dependencies = [](const std::shared_ptr<void>& decoded, const assets::Manager::Request& request) {
            std::vector<assets::Manager::Dependency> needed;
            for (const particles2d::Effect::TextureFile& file : std::static_pointer_cast<particles2d::Effect>(decoded)->getTextureFiles()) {
                needed.push_back({.type = "texture", .path = file.path, .options = textureOptions(file.options, request.options)});
            }

            return needed;
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
            return std::make_shared<particles2d::ImageShape>(graphics::Image::decode(request.bytes, graphics::Image::kMaxSide, request.path), shapeOptionsFromJson(request.options));
        },
        .finalize = [](std::shared_ptr<void> result, const assets::Manager::Request&) { return result; },
    });
    // clang-format on
}

void Particles2DPlugin::stop(core::Engine&) {
    emitters.clear();
    systems.clear();
    trails.clear();
}

bool Particles2DPlugin::isAnimating() const {
    return emitters.any([](const particles2d::Emitter& emitter) { return emitter.isAlive(); }) || systems.any([](const particles2d::System& system) { return system.isAlive(); }) || trails.any([](const particles2d::Trail& trail) { return trail.isAlive(); });
}

void Particles2DPlugin::track(const std::shared_ptr<particles2d::Emitter>& emitter) {
    emitters.add(emitter);
}

void Particles2DPlugin::track(const std::shared_ptr<particles2d::System>& system) {
    systems.add(system);
}

void Particles2DPlugin::track(const std::shared_ptr<particles2d::Trail>& trail) {
    trails.add(trail);
}

} // namespace haylen::plugins
