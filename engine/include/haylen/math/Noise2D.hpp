#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// Seeded noise for procedural content. Gradient noise values are in [-1, 1].
class Noise2D final {
  public:
    enum class Kind : std::uint8_t {
        Perlin,
        Simplex,
        Fractal,
        Worley,
    };

    // A grid of samples, `columns` across and `rows` down, the first at `origin` and the next ones `step` apart on each axis. Fractal samples take the octaves, lacunarity and gain, and Worley samples are the distance to the nearest feature point.
    struct Grid {
        Kind kind = Kind::Fractal;
        Vec2 origin;
        Vec2 step{1.0F, 1.0F};
        std::size_t columns = 0;
        std::size_t rows = 0;
        int octaves = 4;
        float lacunarity = 2.0F;
        float gain = 0.5F;
    };

    static constexpr std::array<std::pair<std::string_view, Kind>, 4> kKinds{{
        {"perlin", Kind::Perlin},
        {"simplex", Kind::Simplex},
        {"fractal", Kind::Fractal},
        {"worley", Kind::Worley},
    }};

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

    [[nodiscard]] static std::optional<Kind> kindFromName(std::string_view name) noexcept;

    [[nodiscard]] float sample(Kind kind, float x, float y, int octaves = 4, float lacunarity = 2.0F, float gain = 0.5F) const noexcept;

    // Writes the samples of the rows from `firstRow` up to `lastRow` of a grid into `values`, which holds the whole grid row after row, so threads fill separate rows of one buffer at once.
    void sampleRows(const Grid& grid, std::size_t firstRow, std::size_t lastRow, std::span<float> values) const noexcept;

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
