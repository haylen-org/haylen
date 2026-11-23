#include "plugins/Animation2DPlugin.hpp"

#include <memory>
#include <string>
#include <vector>

#include "graphics/TextureResource.hpp"
#include "haylen/2d/animation/SkeletonData.hpp"
#include "haylen/2d/animation/SpriteAtlas.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"

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
            decoded->image = graphics::Image::decodeFor(request.package->readAsset(decoded->imagePath), assets::Manager::textureOptionsFromJson(request.options));
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
            decoded->atlas.image = graphics::Image::decodeFor(request.package->readAsset(decoded->atlas.imagePath), assets::Manager::textureOptionsFromJson(request.options));
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
    // clang-format on
}

void Animation2DPlugin::stop(core::Engine&) {
    animators.clear();
    skeletons.clear();
    batches.clear();
}

bool Animation2DPlugin::isAnimating() const {
    return animators.any([](const animation2d::Animator& animator) { return animator.isPlaying() && !animator.isFinished() && animator.speed != 0.0F; }) || skeletons.any([](const animation2d::Skeleton& skeleton) { return skeleton.isPlaying(); }) || batches.any([](const animation2d::AnimatedBatch& batch) { return batch.isPlaying(); });
}

void Animation2DPlugin::track(const std::shared_ptr<animation2d::Animator>& animator) {
    animators.add(animator);
}

void Animation2DPlugin::track(const std::shared_ptr<animation2d::Skeleton>& skeleton) {
    skeletons.add(skeleton);
}

void Animation2DPlugin::track(const std::shared_ptr<animation2d::AnimatedBatch>& batch) {
    batches.add(batch);
}

} // namespace haylen::plugins
