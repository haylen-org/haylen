#pragma once

#include <array>
#include <cstdint>

#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// Seeded noise for procedural content. Gradient noise values are in [-1, 1].
class Noise2D final {
  public:
    // Distances run in cell units from the nearest and second nearest feature points, and `cell` tells the region of the nearest point apart with a value in [0, 1).
    struct Cellular {
        float nearest = 0.0F;
        float second = 0.0F;
        float cell = 0.0F;
    };

    explicit Noise2D(std::uint64_t seed = 0) noexcept;

    [[nodiscard]] float perlin(float x, float y) const noexcept;
    [[nodiscard]] float simplex(float x, float y) const noexcept;
    [[nodiscard]] float fractal(float x, float y, int octaves = 4, float lacunarity = 2.0F, float gain = 0.5F) const noexcept;

    // Worley noise with one jittered feature point per unit cell.
    [[nodiscard]] Cellular worley(float x, float y) const noexcept;

    // Moves the point by fractal noise sampled at `frequency`, up to `amplitude` away, so sampling any noise at the result bends its patterns.
    [[nodiscard]] Vec2 warp(float x, float y, float amplitude, float frequency = 1.0F, int octaves = 3) const noexcept;

  private:
    [[nodiscard]] std::uint8_t hash(int x, int y) const noexcept;
    [[nodiscard]] static float fade(float t) noexcept;
    [[nodiscard]] static float gradientDot(std::uint8_t cornerHash, float x, float y) noexcept;

    static constexpr std::array<std::array<float, 2>, 8> kGradients = {{
        {1.0F, 0.0F},
        {-1.0F, 0.0F},
        {0.0F, 1.0F},
        {0.0F, -1.0F},
        {0.70710678F, 0.70710678F},
        {-0.70710678F, 0.70710678F},
        {0.70710678F, -0.70710678F},
        {-0.70710678F, -0.70710678F},
    }};

    std::array<std::uint8_t, 512> permutation{};
};

} // namespace haylen::math
