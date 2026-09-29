#include "haylen/math/Noise2D.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <span>

#include "haylen/math/Math.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::math {

Noise2D::Noise2D(std::uint64_t seed) noexcept {
    std::array<std::uint8_t, 256> values{};
    std::iota(values.begin(), values.end(), std::uint8_t{0});

    Random random(seed);
    random.shuffle(std::span<std::uint8_t>(values));

    for (std::size_t index = 0; index < permutation.size(); ++index) {
        permutation[index] = values[index & 255U];
    }
}

float Noise2D::fade(float t) noexcept {
    return t * t * t * (t * (t * 6.0F - 15.0F) + 10.0F);
}

float Noise2D::gradientDot(std::uint8_t cornerHash, float x, float y) noexcept {
    const auto& gradient = kGradients[cornerHash & 7U];
    return gradient[0] * x + gradient[1] * y;
}

std::uint8_t Noise2D::hash(int x, int y) const noexcept {
    const auto ix = static_cast<std::size_t>(x & 255);
    const auto iy = static_cast<std::size_t>(y & 255);
    return permutation[permutation[ix] + iy];
}

float Noise2D::perlin(float x, float y) const noexcept {
    const float floorX = std::floor(x);
    const float floorY = std::floor(y);
    const int cellX = static_cast<int>(floorX);
    const int cellY = static_cast<int>(floorY);
    const float localX = x - floorX;
    const float localY = y - floorY;

    const float n00 = gradientDot(hash(cellX, cellY), localX, localY);
    const float n10 = gradientDot(hash(cellX + 1, cellY), localX - 1.0F, localY);
    const float n01 = gradientDot(hash(cellX, cellY + 1), localX, localY - 1.0F);
    const float n11 = gradientDot(hash(cellX + 1, cellY + 1), localX - 1.0F, localY - 1.0F);

    const float u = fade(localX);
    const float v = fade(localY);
    return Math::lerp(Math::lerp(n00, n10, u), Math::lerp(n01, n11, u), v) * 1.41421356F;
}

float Noise2D::simplex(float x, float y) const noexcept {
    constexpr float kSkew = 0.36602540378F;
    constexpr float kUnskew = 0.21132486540F;

    const float skew = (x + y) * kSkew;
    const int i = static_cast<int>(std::floor(x + skew));
    const int j = static_cast<int>(std::floor(y + skew));
    const float unskew = static_cast<float>(i + j) * kUnskew;
    const float x0 = x - (static_cast<float>(i) - unskew);
    const float y0 = y - (static_cast<float>(j) - unskew);

    const int i1 = x0 > y0 ? 1 : 0;
    const int j1 = x0 > y0 ? 0 : 1;
    const float x1 = x0 - static_cast<float>(i1) + kUnskew;
    const float y1 = y0 - static_cast<float>(j1) + kUnskew;
    const float x2 = x0 - 1.0F + 2.0F * kUnskew;
    const float y2 = y0 - 1.0F + 2.0F * kUnskew;

    // clang-format off
    const auto corner = [](std::uint8_t cornerHash, float cx, float cy) {
        const float falloff = 0.5F - cx * cx - cy * cy;
        if (falloff <= 0.0F) {
            return 0.0F;
        }
        const float squared = falloff * falloff;
        return squared * squared * gradientDot(cornerHash, cx, cy);
    };
    // clang-format on

    const float total = corner(hash(i, j), x0, y0) + corner(hash(i + i1, j + j1), x1, y1) + corner(hash(i + 1, j + 1), x2, y2);
    return std::clamp(total * 70.0F, -1.0F, 1.0F);
}

float Noise2D::fractal(float x, float y, int octaves, float lacunarity, float gain) const noexcept {
    float sum = 0.0F;
    float amplitude = 1.0F;
    float frequency = 1.0F;
    float normalization = 0.0F;

    for (int octave = 0; octave < octaves; ++octave) {
        sum += simplex(x * frequency, y * frequency) * amplitude;
        normalization += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return normalization > 0.0F ? sum / normalization : 0.0F;
}

Noise2D::Cellular Noise2D::worley(float x, float y) const noexcept {
    const float floorX = std::floor(x);
    const float floorY = std::floor(y);
    const int cellX = static_cast<int>(floorX);
    const int cellY = static_cast<int>(floorY);

    Cellular result{.nearest = 1e9F, .second = 1e9F};
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            const int column = cellX + offsetX;
            const int row = cellY + offsetY;
            const std::uint8_t first = hash(column, row);
            const std::uint8_t second = hash(column + 101, row + 57);
            const float featureX = static_cast<float>(column) + (static_cast<float>(first) + 0.5F) / 256.0F;
            const float featureY = static_cast<float>(row) + (static_cast<float>(second) + 0.5F) / 256.0F;
            const float distance = std::hypot(featureX - x, featureY - y);

            if (distance < result.nearest) {
                result.second = result.nearest;
                result.nearest = distance;
                result.cell = static_cast<float>(first * 256 + second) / 65536.0F;
            } else if (distance < result.second) {
                result.second = distance;
            }
        }
    }
    return result;
}

Vec2 Noise2D::warp(float x, float y, float amplitude, float frequency, int octaves) const noexcept {
    const float sampleX = x * frequency;
    const float sampleY = y * frequency;
    const float offsetX = fractal(sampleX + 5.2F, sampleY + 1.3F, octaves);
    const float offsetY = fractal(sampleX + 1.7F, sampleY + 9.2F, octaves);
    return {x + offsetX * amplitude, y + offsetY * amplitude};
}

} // namespace haylen::math
