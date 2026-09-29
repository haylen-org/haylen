#include "haylen/math/Spring.hpp"

#include <algorithm>

namespace haylen::math {

Spring::Spring(float initial, float smoothing) noexcept : value(initial), smoothTime(std::max(smoothing, kMinimumSmoothTime)) {}

float Spring::update(float target, float deltaSeconds) noexcept {
    value = smoothDamp(value, target, velocity, smoothTime, deltaSeconds);
    return value;
}

void Spring::setValue(float amount) noexcept {
    value = amount;
}

void Spring::setVelocity(float amount) noexcept {
    velocity = amount;
}

void Spring::setSmoothTime(float seconds) noexcept {
    smoothTime = std::max(seconds, kMinimumSmoothTime);
}

float Spring::decay(float omega, float deltaSeconds) noexcept {
    const float x = omega * deltaSeconds;
    return 1.0F / (1.0F + x + 0.48F * x * x + 0.235F * x * x * x);
}

float Spring::smoothDamp(float current, float target, float& currentVelocity, float smoothing, float deltaSeconds) noexcept {
    if (deltaSeconds <= 0.0F) {
        return current;
    }

    const float omega = 2.0F / std::max(smoothing, kMinimumSmoothTime);
    const float factor = decay(omega, deltaSeconds);
    const float change = current - target;
    const float impulse = (currentVelocity + omega * change) * deltaSeconds;
    currentVelocity = (currentVelocity - omega * impulse) * factor;
    const float result = target + (change + impulse) * factor;

    // Rounding can carry the value past the target, where it stops instead of turning back.
    if ((target - current) * (result - target) > 0.0F) {
        currentVelocity = 0.0F;
        return target;
    }
    return result;
}

Vec2 Spring::smoothDamp(Vec2 current, Vec2 target, Vec2& currentVelocity, float smoothing, float deltaSeconds) noexcept {
    if (deltaSeconds <= 0.0F) {
        return current;
    }

    const float omega = 2.0F / std::max(smoothing, kMinimumSmoothTime);
    const float factor = decay(omega, deltaSeconds);
    const Vec2 change = current - target;
    const Vec2 impulse = (currentVelocity + change * omega) * deltaSeconds;
    currentVelocity = (currentVelocity - impulse * omega) * factor;
    const Vec2 result = target + (change + impulse) * factor;

    if (Vec2::dot(target - current, result - target) > 0.0F) {
        currentVelocity = {};
        return target;
    }
    return result;
}

} // namespace haylen::math
