#pragma once

#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// A critically damped spring that follows a moving target as fast as possible without overshooting it. The smooth time is roughly how long the value takes to reach a target that stands still.
class Spring final {
  public:
    explicit Spring(float initial = 0.0F, float smoothing = 0.2F) noexcept;

    float update(float target, float deltaSeconds) noexcept;

    [[nodiscard]] float getValue() const noexcept {
        return value;
    }
    void setValue(float amount) noexcept;
    [[nodiscard]] float getVelocity() const noexcept {
        return velocity;
    }
    void setVelocity(float amount) noexcept;
    [[nodiscard]] float getSmoothTime() const noexcept {
        return smoothTime;
    }
    void setSmoothTime(float seconds) noexcept;

    // Advances any value toward the target and updates its velocity in place.
    [[nodiscard]] static float smoothDamp(float current, float target, float& currentVelocity, float smoothing, float deltaSeconds) noexcept;
    [[nodiscard]] static Vec2 smoothDamp(Vec2 current, Vec2 target, Vec2& currentVelocity, float smoothing, float deltaSeconds) noexcept;

  private:
    static constexpr float kMinimumSmoothTime = 1e-4F;

    // Returns the decay of the exact solution over the step, approximated by a polynomial that stays accurate for any step.
    [[nodiscard]] static float decay(float omega, float deltaSeconds) noexcept;

    float value;
    float velocity = 0.0F;
    float smoothTime;
};

} // namespace haylen::math
