#pragma once

#include <algorithm>
#include <cmath>

namespace haylen::math {

// Scalar constants and helpers shared by the whole engine.
class Math final {
  public:
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTau = kPi * 2.0F;
    static constexpr float kHalfPi = kPi * 0.5F;

    // Returns from and to exactly at 0 and 1.
    [[nodiscard]] static constexpr float lerp(float from, float to, float t) noexcept {
        return from * (1.0F - t) + to * t;
    }

    [[nodiscard]] static constexpr float inverseLerp(float from, float to, float value) noexcept {
        return from == to ? 0.0F : (value - from) / (to - from);
    }

    [[nodiscard]] static constexpr float remap(float value, float fromMin, float fromMax, float toMin, float toMax) noexcept {
        return lerp(toMin, toMax, inverseLerp(fromMin, fromMax, value));
    }

    [[nodiscard]] static constexpr float saturate(float value) noexcept {
        return std::clamp(value, 0.0F, 1.0F);
    }

    [[nodiscard]] static constexpr float smoothstep(float edge0, float edge1, float value) noexcept {
        const float t = saturate(inverseLerp(edge0, edge1, value));
        return t * t * (3.0F - 2.0F * t);
    }

    [[nodiscard]] static constexpr float moveToward(float current, float target, float maxDelta) noexcept {
        if (current < target) {
            return std::min(current + maxDelta, target);
        }
        return std::max(current - maxDelta, target);
    }

    [[nodiscard]] static constexpr float sign(float value) noexcept {
        return value > 0.0F ? 1.0F : (value < 0.0F ? -1.0F : 0.0F);
    }

    [[nodiscard]] static constexpr float radians(float angle) noexcept {
        return angle * (kPi / 180.0F);
    }

    [[nodiscard]] static constexpr float degrees(float angle) noexcept {
        return angle * (180.0F / kPi);
    }

    [[nodiscard]] static bool approximately(float lhs, float rhs, float epsilon = 1e-5F) noexcept {
        return std::fabs(lhs - rhs) <= epsilon * std::max({1.0F, std::fabs(lhs), std::fabs(rhs)});
    }

    // Returns the angle wrapped into the range [-pi, pi).
    [[nodiscard]] static float wrapAngle(float angle) noexcept {
        const float wrapped = std::fmod(angle + kPi, kTau);
        return (wrapped < 0.0F ? wrapped + kTau : wrapped) - kPi;
    }

    // Returns the interpolation factor for frame-rate independent exponential smoothing at the given rate per second.
    [[nodiscard]] static float dampFactor(float rate, float deltaSeconds) noexcept {
        return 1.0F - std::exp(-rate * deltaSeconds);
    }
};

} // namespace haylen::math
