#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "haylen/math/Noise2D.hpp"

namespace haylen::lighting2d {

// Flame-like wavering of light intensity.
class LightFlicker final {
  public:
    // Returns a light intensity multiplier between `1 - amount` and 1, so it can scale any valid light intensity. The same seed and time always give the same value.
    [[nodiscard]] static float intensity(float time, float speed, float amount, std::uint64_t seed) noexcept;

  private:
    struct Entry {
        std::uint64_t seed = 0;
        math::Noise2D noise;
    };

    static constexpr std::size_t kCachedSeeds = 8;

    // Building the noise of a seed shuffles a whole permutation table, so every thread keeps the noise of the seeds it used last, starting with seed 0 everywhere.
    static thread_local std::array<Entry, kCachedSeeds> entries;
    static thread_local std::size_t nextEntry;

    [[nodiscard]] static const math::Noise2D& noiseFor(std::uint64_t seed) noexcept;
};

} // namespace haylen::lighting2d
