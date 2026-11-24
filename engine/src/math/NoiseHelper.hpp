#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>

#include "haylen/math/Random.hpp"

namespace haylen::math {

// The lattice that the noises of every dimension share: a seeded permutation of 256 cells, the curve that blends between cells and the wrap of a coordinate into a cell.
class NoiseHelper final {
  public:
    // Returns the cells shuffled by the seed, twice over, so a hash may add an index of up to 255 to a value of the table.
    [[nodiscard]] static std::array<std::uint8_t, 512> makePermutation(std::uint64_t seed) noexcept {
        std::array<std::uint8_t, 256> values{};
        std::iota(values.begin(), values.end(), std::uint8_t{0});

        Random random(seed);
        random.shuffle(std::span<std::uint8_t>(values));

        std::array<std::uint8_t, 512> permutation{};
        for (std::size_t index = 0; index < permutation.size(); ++index) {
            permutation[index] = values[index & 255U];
        }

        return permutation;
    }

    [[nodiscard]] static float fade(float t) noexcept {
        return t * t * t * (t * (t * 6.0F - 15.0F) + 10.0F);
    }

    // The lattice repeats every 256 cells, so a cell wraps into that period before it becomes an integer, which keeps coordinates of any size in range. A coordinate that is not finite hashes the first cell, and its sample is not a number either.
    [[nodiscard]] static int cellOf(float floored) noexcept {
        const float wrapped = std::fmod(floored, 256.0F);
        return std::isfinite(wrapped) ? static_cast<int>(wrapped) : 0;
    }
};

} // namespace haylen::math
