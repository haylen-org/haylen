#pragma once

#include <memory>
#include <vector>

#include "haylen/core/Tween.hpp"
#include "haylen/core/TweenTrack.hpp"
#include "haylen/math/EasingCurve.hpp"

namespace haylen::core {

// A tween that animates values through its tracks with an easing curve. Without tracks it only takes time, which is how timelines hold intervals.
class PropertyTween final : public Tween {
  public:
    // A speed-based tween reads the length as units per second and finds its duration from the distance its values travel when it starts.
    explicit PropertyTween(float length, bool perSecond = false);

    void addTrack(std::unique_ptr<TweenTrack> track);
    [[nodiscard]] const std::vector<std::unique_ptr<TweenTrack>>& getTracks() const noexcept {
        return tracks;
    }

    void setEase(math::EasingCurve value) {
        ease = std::move(value);
    }
    [[nodiscard]] const math::EasingCurve& getEase() const noexcept {
        return ease;
    }

    [[nodiscard]] bool isSpeedBased() const noexcept {
        return speedBased;
    }

    // Resolves every track and shows the start values right away, which from tweens do so the target does not show its end values while the delay runs.
    void renderStart();

    [[nodiscard]] float getDuration() const noexcept override {
        return duration;
    }

  protected:
    void prepare() override;
    void renderLoop(float loopTime, float previous, int loops, bool silent) override;
    [[nodiscard]] float getRenderedProgress() const noexcept override {
        return easedProgress;
    }
    bool killTarget(const void* target) override;
    void yieldTo(const PropertyTween& newer) override;
    void discard() noexcept override;

  private:
    [[nodiscard]] bool areTargetsAlive() const;

    std::vector<std::unique_ptr<TweenTrack>> tracks;
    math::EasingCurve ease;
    float duration;
    float speed = 0.0F;
    float easedProgress = 0.0F;
    bool speedBased;
    bool begun = false;
};

} // namespace haylen::core
