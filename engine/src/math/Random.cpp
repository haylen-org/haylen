#include "haylen/math/Random.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

#include "haylen/math/Math.hpp"

namespace haylen::math {

Random::Random(std::uint64_t seed) noexcept {
    reseed(seed);
}

std::uint64_t Random::splitMix(std::uint64_t& seed) noexcept {
    std::uint64_t value = (seed += 0x9E3779B97F4A7C15ULL);
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

void Random::reseed(std::uint64_t seed) noexcept {
    for (auto& word : state) {
        word = splitMix(seed);
    }
}

void Random::setState(const std::array<std::uint64_t, 4>& value) {
    if (value == std::array<std::uint64_t, 4>{}) {
        throw std::invalid_argument("A random state cannot be all zeros.");
    }

    state = value;
}

std::uint64_t Random::nextU64() noexcept {
    const std::uint64_t result = std::rotl(state[1] * 5U, 7) * 9U;
    const std::uint64_t shifted = state[1] << 17U;

    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];
    state[2] ^= shifted;
    state[3] = std::rotl(state[3], 45);
    return result;
}

std::uint32_t Random::nextU32() noexcept {
    return static_cast<std::uint32_t>(nextU64() >> 32U);
}

float Random::nextFloat() noexcept {
    return static_cast<float>(nextU64() >> 40U) * (1.0F / 16777216.0F);
}

float Random::range(float minimum, float maximum) noexcept {
    return minimum + (maximum - minimum) * nextFloat();
}

int Random::range(int minimum, int maximum) noexcept {
    if (maximum <= minimum) {
        return minimum;
    }

    const auto span = static_cast<std::uint64_t>(static_cast<std::int64_t>(maximum) - minimum + 1);
    return static_cast<int>(static_cast<std::int64_t>(minimum) + static_cast<std::int64_t>(nextU64() % span));
}

bool Random::chance(float probability) noexcept {
    return nextFloat() < probability;
}

std::size_t Random::weightedIndex(std::span<const float> weights) {
    double total = 0.0;
    for (const float weight : weights) {
        if (!std::isfinite(weight) || weight < 0.0F) {
            throw std::invalid_argument("Weights must be finite and not negative.");
        }

        total += static_cast<double>(weight);
    }

    if (total <= 0.0) {
        throw std::invalid_argument("A weighted pick needs at least one positive weight.");
    }

    // Rounding can carry the target past the last weight, and the pick then falls to the last positive weight.
    double target = static_cast<double>(nextFloat()) * total;
    std::size_t lastPositive = 0;
    for (std::size_t index = 0; index < weights.size(); ++index) {
        if (weights[index] <= 0.0F) {
            continue;
        }

        if (target < static_cast<double>(weights[index])) {
            return index;
        }

        target -= static_cast<double>(weights[index]);
        lastPositive = index;
    }

    return lastPositive;
}

// The height along the axis of a point of the sphere spreads evenly, which spreads the points evenly over its area.
Vec3 Random::onSphere(float radius) noexcept {
    const float height = range(-1.0F, 1.0F);
    const float turn = range(0.0F, Math::kTau);
    const float across = std::sqrt(std::max(0.0F, 1.0F - height * height));
    return Vec3{across * std::cos(turn), across * std::sin(turn), height} * radius;
}

// The cube root of the distance from the center spreads the points evenly through the volume, which grows with the cube of the radius.
Vec3 Random::inSphere(float radius) noexcept {
    return onSphere(radius) * std::cbrt(nextFloat());
}

Vec3 Random::inCone(Vec3 axis, float angle) {
    if (!(angle >= 0.0F && angle <= Math::kPi)) {
        throw std::invalid_argument("A random cone needs an angle from 0 to pi radians.");
    }

    // The cosine of the angle from the axis spreads evenly over the cap, and two directions across the axis place the turn around it.
    const Vec3 unit = axis.getNormalized();
    const Vec3 helper = std::fabs(unit.x) < 0.9F ? Vec3::right() : Vec3::up();
    const Vec3 first = Vec3::cross(unit, helper).getNormalized();
    const Vec3 second = Vec3::cross(unit, first);
    const float cosine = range(std::cos(angle), 1.0F);
    const float sine = std::sqrt(std::max(0.0F, 1.0F - cosine * cosine));
    const float turn = range(0.0F, Math::kTau);
    return unit * cosine + (first * std::cos(turn) + second * std::sin(turn)) * sine;
}

Vec3 Random::inBox(const Box& box) noexcept {
    const float x = range(box.min.x, box.max.x);
    const float y = range(box.min.y, box.max.y);
    const float z = range(box.min.z, box.max.z);
    return {x, y, z};
}

} // namespace haylen::math
