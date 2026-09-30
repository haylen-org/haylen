#include "haylen/core/TweenMotion.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "haylen/math/Math.hpp"

namespace haylen::core {

std::shared_ptr<TweenMotion> TweenMotion::jump(float power, int jumps) {
    if (jumps < 1) {
        throw std::invalid_argument("A jump needs at least one hop.");
    }
    auto motion = std::shared_ptr<TweenMotion>(new TweenMotion(Kind::Jump));
    motion->amount = power;
    motion->count = jumps;
    return motion;
}

std::shared_ptr<TweenMotion> TweenMotion::path(std::vector<math::Vec2> waypoints, bool curved, bool closed) {
    if (waypoints.empty()) {
        throw std::invalid_argument("A path needs at least one point.");
    }
    auto motion = std::shared_ptr<TweenMotion>(new TweenMotion(Kind::Path));
    motion->points = std::move(waypoints);
    motion->curvedPath = curved;
    motion->closedPath = closed;
    return motion;
}

std::shared_ptr<TweenMotion> TweenMotion::bezier(std::vector<math::Vec2> controls) {
    if (controls.empty() || controls.size() > 2) {
        throw std::invalid_argument("A Bézier curve needs one or two control points.");
    }
    auto motion = std::shared_ptr<TweenMotion>(new TweenMotion(Kind::Bezier));
    motion->points = std::move(controls);
    return motion;
}

std::shared_ptr<TweenMotion> TweenMotion::shake(int vibrato, float randomness, std::uint32_t randomSeed) {
    if (vibrato < 1) {
        throw std::invalid_argument("A shake needs a vibrato of at least 1.");
    }
    auto motion = std::shared_ptr<TweenMotion>(new TweenMotion(Kind::Shake));
    motion->count = vibrato;
    motion->amount = std::clamp(randomness, 0.0F, 180.0F);
    motion->seed = randomSeed;
    return motion;
}

std::shared_ptr<TweenMotion> TweenMotion::punch(int vibrato, float elasticity) {
    if (vibrato < 1) {
        throw std::invalid_argument("A punch needs a vibrato of at least 1.");
    }
    auto motion = std::shared_ptr<TweenMotion>(new TweenMotion(Kind::Punch));
    motion->count = vibrato;
    motion->amount = std::clamp(elasticity, 0.0F, 1.0F);
    return motion;
}

std::shared_ptr<TweenMotion> TweenMotion::blink(int times) {
    if (times < 1) {
        throw std::invalid_argument("A blink needs at least one blink.");
    }
    auto motion = std::shared_ptr<TweenMotion>(new TweenMotion(Kind::Blink));
    motion->count = times;
    return motion;
}

std::shared_ptr<TweenMotion> TweenMotion::orientation(std::shared_ptr<TweenMotion> followed) {
    if (!followed || followed->kind != Kind::Path) {
        throw std::invalid_argument("An orientation follows a path motion.");
    }
    auto motion = std::shared_ptr<TweenMotion>(new TweenMotion(Kind::Orientation));
    motion->source = std::move(followed);
    return motion;
}

float TweenMotion::hash(std::uint32_t key, std::uint32_t index) noexcept {
    std::uint32_t value = key * 0x9E3779B9U + index * 0x85EBCA6BU + 0x632BE5ABU;
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    value *= 0x846CA68BU;
    value ^= value >> 16U;
    return static_cast<float>(value >> 8U) / static_cast<float>(1U << 24U);
}

math::Vec2 TweenMotion::vector(const TweenValue& value) {
    if (value.getKind() != TweenValue::Kind::Vector) {
        throw std::invalid_argument("This motion only moves \"Vec2\" values.");
    }
    return value.getVector();
}

void TweenMotion::prepare(const TweenValue& from, const TweenValue& to) {
    if (kind == Kind::Jump || kind == Kind::Bezier) {
        (void)vector(from);
        (void)vector(to);
        return;
    }
    if (kind != Kind::Path) {
        return;
    }

    nodes.clear();
    nodes.push_back(vector(from));
    nodes.insert(nodes.end(), points.begin(), points.end());
    if (closedPath) {
        nodes.push_back(nodes.front());
    }

    // The table maps the travelled distance to the curve parameter, so the path moves at constant speed however its points are spaced.
    const auto samples = static_cast<int>(nodes.size() - 1) * kPathSamples;
    lengths.assign(static_cast<std::size_t>(samples) + 1, 0.0F);
    math::Vec2 previous = pathPoint(0.0F);
    for (int sample = 1; sample <= samples; ++sample) {
        const math::Vec2 current = pathPoint(static_cast<float>(sample) / static_cast<float>(samples));
        lengths[static_cast<std::size_t>(sample)] = lengths[static_cast<std::size_t>(sample) - 1] + math::Vec2::distance(previous, current);
        previous = current;
    }
}

math::Vec2 TweenMotion::pathPoint(float t) const noexcept {
    const auto segments = static_cast<int>(nodes.size()) - 1;
    if (segments <= 0) {
        return nodes.front();
    }
    const float position = std::clamp(t, 0.0F, 1.0F) * static_cast<float>(segments);
    const int segment = std::min(static_cast<int>(position), segments - 1);
    const float f = position - static_cast<float>(segment);
    const math::Vec2 p1 = nodes[static_cast<std::size_t>(segment)];
    const math::Vec2 p2 = nodes[static_cast<std::size_t>(segment) + 1];
    if (!curvedPath) {
        return math::Vec2::lerp(p1, p2, f);
    }

    // A closed path wraps its neighbours around, and an open one repeats its end points.
    const int last = segments;
    const int before = segment > 0 ? segment - 1 : (closedPath ? last - 1 : 0);
    const int after = segment + 2 <= last ? segment + 2 : (closedPath ? 1 : last);
    const math::Vec2 p0 = nodes[static_cast<std::size_t>(before)];
    const math::Vec2 p3 = nodes[static_cast<std::size_t>(after)];
    const float f2 = f * f;
    const float f3 = f2 * f;
    return (p1 * 2.0F + (p2 - p0) * f + (p0 * 2.0F - p1 * 5.0F + p2 * 4.0F - p3) * f2 + (p1 * 3.0F - p0 - p2 * 3.0F + p3) * f3) * 0.5F;
}

math::Vec2 TweenMotion::pathAt(float progress) const noexcept {
    const float target = std::clamp(progress, 0.0F, 1.0F) * lengths.back();
    const auto after = std::lower_bound(lengths.begin(), lengths.end(), target);
    if (after == lengths.begin()) {
        return nodes.front();
    }
    if (after == lengths.end()) {
        return nodes.back();
    }

    const auto index = static_cast<float>(after - lengths.begin());
    const float span = *after - *(after - 1);
    const float fraction = span > 0.0F ? (target - *(after - 1)) / span : 0.0F;
    return pathPoint((index - 1.0F + fraction) / static_cast<float>(lengths.size() - 1));
}

float TweenMotion::pathHeading(float progress) const noexcept {
    constexpr float kStep = 1e-3F;
    const math::Vec2 behind = pathAt(std::max(progress - kStep, 0.0F));
    const math::Vec2 ahead = pathAt(std::min(progress + kStep, 1.0F));
    return (ahead - behind).getAngle();
}

TweenValue TweenMotion::shakeOffset(const TweenValue& strength, float progress) const {
    const float position = std::clamp(progress, 0.0F, 1.0F) * static_cast<float>(count);
    const int step = std::min(static_cast<int>(position), count - 1);
    const float fraction = position - static_cast<float>(step);

    // Each shake swings roughly opposite to the previous one, bent by the randomness, and the first and last points rest at the start value.
    // clang-format off
    const auto offsetAt = [this, &strength](int index) -> TweenValue {
        const bool resting = index <= 0 || index >= count;
        const float decay = resting ? 0.0F : 1.0F - static_cast<float>(index) / static_cast<float>(count);
        const float random = hash(seed, static_cast<std::uint32_t>(index));
        if (strength.getKind() == TweenValue::Kind::Number) {
            const float sign = index % 2 == 0 ? 1.0F : -1.0F;
            return static_cast<double>(sign * decay * (1.0F - 0.5F * random * amount / 180.0F)) * strength.getNumber();
        }
        const math::Vec2 reach = strength.getVector();
        const float angle = hash(seed, 0U) * math::Math::kTau + static_cast<float>(index) * math::Math::kPi + (random * 2.0F - 1.0F) * math::Math::radians(amount);
        return math::Vec2{std::cos(angle) * reach.x, std::sin(angle) * reach.y} * decay;
    };
    // clang-format on
    const TweenValue start = offsetAt(step);
    return TweenValue::mix(start, offsetAt(step + 1), fraction, TweenValue::Interpolation::Linear);
}

TweenValue TweenMotion::evaluate(const TweenValue& from, const TweenValue& to, float progress) const {
    switch (kind) {
    case Kind::Jump: {
        const float hop = progress * static_cast<float>(count) - std::floor(progress * static_cast<float>(count));
        const math::Vec2 base = math::Vec2::lerp(vector(from), vector(to), progress);
        return math::Vec2{base.x, base.y - amount * 4.0F * hop * (1.0F - hop)};
    }
    case Kind::Path:
        return pathAt(progress);
    case Kind::Bezier: {
        const math::Vec2 start = vector(from);
        const math::Vec2 end = vector(to);
        const float inverse = 1.0F - progress;
        if (points.size() == 1) {
            return start * (inverse * inverse) + points[0] * (2.0F * inverse * progress) + end * (progress * progress);
        }
        return start * (inverse * inverse * inverse) + points[0] * (3.0F * inverse * inverse * progress) + points[1] * (3.0F * inverse * progress * progress) + end * (progress * progress * progress);
    }
    case Kind::Shake:
        return TweenValue::add(from, shakeOffset(to, progress));
    case Kind::Punch: {
        float wave = std::sin(progress * static_cast<float>(count) * math::Math::kPi) * (1.0F - progress);
        if (wave < 0.0F) {
            wave *= amount;
        }
        return TweenValue::add(from, to, wave);
    }
    case Kind::Blink: {
        const auto phase = static_cast<int>(std::floor(std::clamp(progress, 0.0F, 1.0F) * static_cast<float>(count * 2)));
        return phase % 2 == 0 ? from : to;
    }
    case Kind::Orientation:
        return static_cast<double>(source->pathHeading(progress));
    }
    return from;
}

float TweenMotion::getDistance(const TweenValue& from, const TweenValue& to) const {
    switch (kind) {
    case Kind::Path:
        return lengths.empty() ? 0.0F : lengths.back();
    case Kind::Bezier: {
        float length = 0.0F;
        math::Vec2 previous = vector(from);
        for (int sample = 1; sample <= kPathSamples; ++sample) {
            const math::Vec2 current = evaluate(from, to, static_cast<float>(sample) / static_cast<float>(kPathSamples)).getVector();
            length += math::Vec2::distance(previous, current);
            previous = current;
        }
        return length;
    }
    case Kind::Shake:
    case Kind::Punch:
        return to.getKind() == TweenValue::Kind::Number ? static_cast<float>(std::fabs(to.getNumber())) : to.getVector().getLength();
    case Kind::Blink:
        return static_cast<float>(count);
    case Kind::Orientation:
        return 0.0F;
    case Kind::Jump:
        break;
    }
    return TweenValue::distance(from, to, TweenValue::Interpolation::Linear);
}

} // namespace haylen::core
