#pragma once

#include <memory>

#include "haylen/2d/animation/AnimatedBatch.hpp"
#include "haylen/2d/animation/Animator.hpp"
#include "haylen/2d/animation/Skeleton.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "plugins/LiveObjects.hpp"

namespace haylen::plugins {

// Registers the `atlas` asset type for sprite atlas JSON, whose image shares the texture cache, and the `skeleton` asset type for `.skeleton` files, whose atlas shares the atlas cache, and installs the `haylen.animation2d` module. It follows the animators, skeletons and animated batches that Lua creates, so an app that redraws on change draws while one of them plays.
class Animation2DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "animation2d";
    }
    void start(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
    void stop(core::Engine& engine) override;

    // Whether an animator, a skeleton or an animated batch plays, which its next update moves.
    [[nodiscard]] bool isAnimating() const override;

    void track(const std::shared_ptr<animation2d::Animator>& animator);
    void track(const std::shared_ptr<animation2d::Skeleton>& skeleton);
    void track(const std::shared_ptr<animation2d::AnimatedBatch>& batch);

  private:
    struct DecodedAtlas;
    struct DecodedSkeleton;

    LiveObjects<animation2d::Animator> animators;
    LiveObjects<animation2d::Skeleton> skeletons;
    LiveObjects<animation2d::AnimatedBatch> batches;
};

} // namespace haylen::plugins
