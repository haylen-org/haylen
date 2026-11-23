#pragma once

#include <memory>

#include "haylen/2d/particles/Emitter.hpp"
#include "haylen/2d/particles/ImageShape.hpp"
#include "haylen/2d/particles/System.hpp"
#include "haylen/2d/particles/Trail.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/plugins/LiveObjects.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Registers the particle effect asset type for `.particles` files, whose textures load as textures of the cache before the effect is finalized, and the `imageShape` asset type that image shapes of emitters load as. It follows the emitters, systems and trails that the app creates, so an app that redraws on change draws while one of them lives.
class Particles2DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "particles2d";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;

    // Whether an emitter, a system or a trail lives, which its next update moves.
    [[nodiscard]] bool isAnimating() const override;

    void track(const std::shared_ptr<particles2d::Emitter>& emitter);
    void track(const std::shared_ptr<particles2d::System>& system);
    void track(const std::shared_ptr<particles2d::Trail>& trail);

  private:
    [[nodiscard]] static core::Json normalizeEffectOptions(const core::Json& options);
    [[nodiscard]] static core::Json textureOptions(const core::Json& fileOptions, const core::Json& loadOptions);
    [[nodiscard]] static particles2d::ImageShape::Options shapeOptionsFromJson(const core::Json& options);

    LiveObjects<particles2d::Emitter> emitters;
    LiveObjects<particles2d::System> systems;
    LiveObjects<particles2d::Trail> trails;
};

} // namespace haylen::plugins
