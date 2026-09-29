#include "haylen/math/Random.hpp"

#include <bit>
#include <cmath>
#include <stdexcept>

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

} // namespace haylen::math
