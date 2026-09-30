#include "haylen/math/Easing.hpp"

#include <algorithm>
#include <cmath>

#include "haylen/math/Math.hpp"

namespace haylen::math {

const std::array<std::pair<std::string_view, Easing::Type>, 31> Easing::kNames = {{
    {"linear", Type::Linear}, {"sineIn", Type::SineIn}, {"sineOut", Type::SineOut}, {"sineInOut", Type::SineInOut}, {"quadIn", Type::QuadIn}, {"quadOut", Type::QuadOut}, {"quadInOut", Type::QuadInOut}, {"cubicIn", Type::CubicIn}, {"cubicOut", Type::CubicOut}, {"cubicInOut", Type::CubicInOut}, {"quartIn", Type::QuartIn}, {"quartOut", Type::QuartOut}, {"quartInOut", Type::QuartInOut}, {"quintIn", Type::QuintIn}, {"quintOut", Type::QuintOut}, {"quintInOut", Type::QuintInOut}, {"expoIn", Type::ExpoIn}, {"expoOut", Type::ExpoOut}, {"expoInOut", Type::ExpoInOut}, {"circIn", Type::CircIn}, {"circOut", Type::CircOut}, {"circInOut", Type::CircInOut}, {"backIn", Type::BackIn}, {"backOut", Type::BackOut}, {"backInOut", Type::BackInOut}, {"elasticIn", Type::ElasticIn}, {"elasticOut", Type::ElasticOut}, {"elasticInOut", Type::ElasticInOut}, {"bounceIn", Type::BounceIn}, {"bounceOut", Type::BounceOut}, {"bounceInOut", Type::BounceInOut},
}};

const std::array<std::pair<std::string_view, Easing::StepPosition>, 4> Easing::kStepPositions = {{
    {"start", StepPosition::Start},
    {"end", StepPosition::End},
    {"both", StepPosition::Both},
    {"none", StepPosition::None},
}};

float Easing::bounceOut(float t) noexcept {
    constexpr float n = 7.5625F;
    constexpr float d = 2.75F;

    if (t < 1.0F / d) {
        return n * t * t;
    }
    if (t < 2.0F / d) {
        t -= 1.5F / d;
        return n * t * t + 0.75F;
    }
    if (t < 2.5F / d) {
        t -= 2.25F / d;
        return n * t * t + 0.9375F;
    }
    t -= 2.625F / d;
    return n * t * t + 0.984375F;
}

float Easing::inOut(float t, Curve curve) noexcept {
    return t < 0.5F ? curve(t * 2.0F) * 0.5F : 1.0F - curve((1.0F - t) * 2.0F) * 0.5F;
}

float Easing::out(float t, Curve curve) noexcept {
    return 1.0F - curve(1.0F - t);
}

float Easing::power(float t, int exponent) noexcept {
    return std::pow(t, static_cast<float>(exponent));
}

float Easing::sineIn(float t) noexcept {
    return 1.0F - std::cos(t * Math::kHalfPi);
}

float Easing::quadIn(float t) noexcept {
    return power(t, 2);
}

float Easing::cubicIn(float t) noexcept {
    return power(t, 3);
}

float Easing::quartIn(float t) noexcept {
    return power(t, 4);
}

float Easing::quintIn(float t) noexcept {
    return power(t, 5);
}

float Easing::expoIn(float t) noexcept {
    return t <= 0.0F ? 0.0F : std::pow(2.0F, 10.0F * t - 10.0F);
}

float Easing::circIn(float t) noexcept {
    return 1.0F - std::sqrt(1.0F - t * t);
}

float Easing::backIn(float t, float overshoot) noexcept {
    return (overshoot + 1.0F) * t * t * t - overshoot * t * t;
}

// Penner's elastic curve. An amplitude below 1 cannot reach the end value, so it counts as 1.
float Easing::elasticIn(float t, float amplitude, float period) noexcept {
    if (t <= 0.0F || t >= 1.0F) {
        return t;
    }
    const float safePeriod = std::max(period, 1e-4F);
    const float safeAmplitude = std::max(amplitude, 1.0F);
    const float shift = safePeriod / Math::kTau * std::asin(1.0F / safeAmplitude);
    const float x = t - 1.0F;
    return -(safeAmplitude * std::pow(2.0F, 10.0F * x) * std::sin((x - shift) * Math::kTau / safePeriod));
}

float Easing::bezierComponent(float u, float first, float second) noexcept {
    const float inverse = 1.0F - u;
    return 3.0F * inverse * inverse * u * first + 3.0F * inverse * u * u * second + u * u * u;
}

float Easing::bezierSlope(float u, float first, float second) noexcept {
    const float inverse = 1.0F - u;
    return 3.0F * inverse * inverse * first + 6.0F * inverse * u * (second - first) + 3.0F * u * u * (1.0F - second);
}

float Easing::bounceIn(float t) noexcept {
    return 1.0F - bounceOut(1.0F - t);
}

float Easing::apply(Type curve, float t) noexcept {
    t = Math::saturate(t);

    switch (curve) {
    case Type::Linear:
        return t;
    case Type::SineIn:
        return sineIn(t);
    case Type::SineOut:
        return out(t, sineIn);
    case Type::SineInOut:
        return inOut(t, sineIn);
    case Type::QuadIn:
        return quadIn(t);
    case Type::QuadOut:
        return out(t, quadIn);
    case Type::QuadInOut:
        return inOut(t, quadIn);
    case Type::CubicIn:
        return cubicIn(t);
    case Type::CubicOut:
        return out(t, cubicIn);
    case Type::CubicInOut:
        return inOut(t, cubicIn);
    case Type::QuartIn:
        return quartIn(t);
    case Type::QuartOut:
        return out(t, quartIn);
    case Type::QuartInOut:
        return inOut(t, quartIn);
    case Type::QuintIn:
        return quintIn(t);
    case Type::QuintOut:
        return out(t, quintIn);
    case Type::QuintInOut:
        return inOut(t, quintIn);
    case Type::ExpoIn:
        return expoIn(t);
    case Type::ExpoOut:
        return out(t, expoIn);
    case Type::ExpoInOut:
        return inOut(t, expoIn);
    case Type::CircIn:
        return circIn(t);
    case Type::CircOut:
        return out(t, circIn);
    case Type::CircInOut:
        return inOut(t, circIn);
    case Type::BackIn:
    case Type::BackOut:
    case Type::BackInOut:
        return apply(curve, t, kBackOvershoot, 0.0F);
    case Type::ElasticIn:
    case Type::ElasticOut:
    case Type::ElasticInOut:
        return apply(curve, t, kElasticAmplitude, kElasticPeriod);
    case Type::BounceIn:
        return bounceIn(t);
    case Type::BounceOut:
        return bounceOut(t);
    case Type::BounceInOut:
        return inOut(t, bounceIn);
    }
    return t;
}

float Easing::apply(Type curve, float t, float first, float second) noexcept {
    t = Math::saturate(t);

    switch (curve) {
    case Type::BackIn:
    case Type::BackOut:
    case Type::BackInOut:
        return shape(curve, t, [first](float x) { return backIn(x, first); });
    case Type::ElasticIn:
    case Type::ElasticOut:
    case Type::ElasticInOut:
        return shape(curve, t, [first, second](float x) { return elasticIn(x, first, second); });
    default:
        return apply(curve, t);
    }
}

float Easing::steps(float t, int count, StepPosition position) noexcept {
    t = Math::saturate(t);
    const int stepCount = std::max(count, 1);
    auto step = static_cast<int>(std::floor(t * static_cast<float>(stepCount)));
    if (position == StepPosition::Start || position == StepPosition::Both) {
        ++step;
    }

    int jumps = stepCount;
    if (position == StepPosition::Both) {
        jumps = stepCount + 1;
    } else if (position == StepPosition::None) {
        jumps = std::max(stepCount - 1, 1);
    }
    return std::clamp(static_cast<float>(step) / static_cast<float>(jumps), 0.0F, 1.0F);
}

// Finds the curve parameter whose x is `t` with Newton steps, which converge fast on well-behaved curves, and falls back to bisection when the slope flattens.
float Easing::cubicBezier(float t, float x1, float y1, float x2, float y2) noexcept {
    t = Math::saturate(t);
    x1 = Math::saturate(x1);
    x2 = Math::saturate(x2);

    float u = t;
    for (int iteration = 0; iteration < 8; ++iteration) {
        const float error = bezierComponent(u, x1, x2) - t;
        const float slope = bezierSlope(u, x1, x2);
        if (std::fabs(error) < 1e-6F) {
            return bezierComponent(u, y1, y2);
        }
        if (std::fabs(slope) < 1e-6F) {
            break;
        }
        u -= error / slope;
    }

    float low = 0.0F;
    float high = 1.0F;
    u = t;
    for (int iteration = 0; iteration < 32; ++iteration) {
        const float x = bezierComponent(u, x1, x2);
        if (std::fabs(x - t) < 1e-6F) {
            break;
        }
        if (x < t) {
            low = u;
        } else {
            high = u;
        }
        u = (low + high) * 0.5F;
    }
    return bezierComponent(u, y1, y2);
}

std::optional<Easing::StepPosition> Easing::parseStepPosition(std::string_view text) noexcept {
    for (const auto& [candidate, position] : kStepPositions) {
        if (candidate == text) {
            return position;
        }
    }
    return std::nullopt;
}

std::string_view Easing::stepPositionName(StepPosition position) noexcept {
    return kStepPositions[static_cast<std::size_t>(position)].first;
}

std::optional<Easing::Type> Easing::parse(std::string_view text) noexcept {
    for (const auto& [candidate, curve] : kNames) {
        if (candidate == text) {
            return curve;
        }
    }
    return std::nullopt;
}

std::string_view Easing::name(Type curve) noexcept {
    return kNames[static_cast<std::size_t>(curve)].first;
}

} // namespace haylen::math
