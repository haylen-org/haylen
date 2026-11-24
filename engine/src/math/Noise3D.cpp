#include "haylen/math/Noise3D.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

#include "haylen/math/Math.hpp"
#include "math/NoiseHelper.hpp"

namespace haylen::math {

const std::array<std::pair<std::string_view, Noise3D::Basis>, 3> Noise3D::kBasisNames{{{"perlin", Basis::Perlin}, {"simplex", Basis::Simplex}, {"value", Basis::Value}}};

const std::array<std::pair<std::string_view, Noise3D::Fractal>, 4> Noise3D::kFractalNames{{{"fbm", Fractal::Fbm}, {"billow", Fractal::Billow}, {"ridged", Fractal::Ridged}, {"pingPong", Fractal::PingPong}}};

Noise3D::Noise3D(std::uint64_t seed) noexcept : permutation(NoiseHelper::makePermutation(seed)) {}

std::optional<Noise3D::Basis> Noise3D::basisFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kBasisNames, name, &std::pair<std::string_view, Basis>::first);
    return found != kBasisNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Noise3D::basisName(Basis value) noexcept {
    return std::ranges::find(kBasisNames, value, &std::pair<std::string_view, Basis>::second)->first;
}

std::optional<Noise3D::Fractal> Noise3D::fractalFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kFractalNames, name, &std::pair<std::string_view, Fractal>::first);
    return found != kFractalNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Noise3D::fractalName(Fractal value) noexcept {
    return std::ranges::find(kFractalNames, value, &std::pair<std::string_view, Fractal>::second)->first;
}

std::uint8_t Noise3D::hash(int x, int y, int z) const noexcept {
    const std::uint8_t xy = permutation[permutation[static_cast<std::size_t>(x & 255)] + static_cast<std::size_t>(y & 255)];
    return permutation[xy + static_cast<std::size_t>(z & 255)];
}

std::uint8_t Noise3D::hash(int x, int y, int z, int w) const noexcept {
    return permutation[hash(x, y, z) + static_cast<std::size_t>(w & 255)];
}

float Noise3D::gradientDot(std::uint8_t cornerHash, float x, float y, float z) noexcept {
    const auto& gradient = kGradients3[cornerHash % kGradients3.size()];
    return gradient[0] * x + gradient[1] * y + gradient[2] * z;
}

float Noise3D::gradientDot(std::uint8_t cornerHash, float x, float y, float z, float w) noexcept {
    const auto& gradient = kGradients4[cornerHash % kGradients4.size()];
    return gradient[0] * x + gradient[1] * y + gradient[2] * z + gradient[3] * w;
}

float Noise3D::latticeValue(std::uint8_t cornerHash) noexcept {
    return static_cast<float>(cornerHash) / 127.5F - 1.0F;
}

float Noise3D::perlin(Vec3 point) const noexcept {
    const Vec3 floored{std::floor(point.x), std::floor(point.y), std::floor(point.z)};
    const int x = NoiseHelper::cellOf(floored.x);
    const int y = NoiseHelper::cellOf(floored.y);
    const int z = NoiseHelper::cellOf(floored.z);
    const Vec3 local = point - floored;
    const float u = NoiseHelper::fade(local.x);
    const float v = NoiseHelper::fade(local.y);
    const float w = NoiseHelper::fade(local.z);

    // Each corner of the cell gives the dot product of its gradient with the offset from that corner.
    std::array<float, 8> corners{};
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
        const int dx = static_cast<int>(corner & 1U);
        const int dy = static_cast<int>((corner >> 1U) & 1U);
        const int dz = static_cast<int>((corner >> 2U) & 1U);
        corners[corner] = gradientDot(hash(x + dx, y + dy, z + dz), local.x - static_cast<float>(dx), local.y - static_cast<float>(dy), local.z - static_cast<float>(dz));
    }

    const float bottom = Math::lerp(Math::lerp(corners[0], corners[1], u), Math::lerp(corners[2], corners[3], u), v);
    const float top = Math::lerp(Math::lerp(corners[4], corners[5], u), Math::lerp(corners[6], corners[7], u), v);
    return std::clamp(Math::lerp(bottom, top, w), -1.0F, 1.0F);
}

float Noise3D::perlin(Vec4 point) const noexcept {
    const Vec4 floored{std::floor(point.x), std::floor(point.y), std::floor(point.z), std::floor(point.w)};
    const std::array<int, 4> cell{NoiseHelper::cellOf(floored.x), NoiseHelper::cellOf(floored.y), NoiseHelper::cellOf(floored.z), NoiseHelper::cellOf(floored.w)};
    const Vec4 local = point - floored;
    const std::array<float, 4> fades{NoiseHelper::fade(local.x), NoiseHelper::fade(local.y), NoiseHelper::fade(local.z), NoiseHelper::fade(local.w)};

    std::array<float, 16> corners{};
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
        const std::array<int, 4> step{static_cast<int>(corner & 1U), static_cast<int>((corner >> 1U) & 1U), static_cast<int>((corner >> 2U) & 1U), static_cast<int>((corner >> 3U) & 1U)};
        const std::uint8_t cornerHash = hash(cell[0] + step[0], cell[1] + step[1], cell[2] + step[2], cell[3] + step[3]);
        corners[corner] = gradientDot(cornerHash, local.x - static_cast<float>(step[0]), local.y - static_cast<float>(step[1]), local.z - static_cast<float>(step[2]), local.w - static_cast<float>(step[3]));
    }

    // Each axis blends the corners in pairs, halving them until one value is left.
    std::size_t count = corners.size();
    for (const float fade : fades) {
        count /= 2;
        for (std::size_t index = 0; index < count; ++index) {
            corners[index] = Math::lerp(corners[index * 2], corners[index * 2 + 1], fade);
        }
    }

    return std::clamp(corners[0], -1.0F, 1.0F);
}

// The simplex noise of Perlin as Gustavson describes it: the space skews so the cube of each cell splits into six tetrahedra, and each corner of the tetrahedron that holds the point adds a falloff times its gradient.
float Noise3D::simplex(Vec3 point) const noexcept {
    constexpr float kSkew = 1.0F / 3.0F;
    constexpr float kUnskew = 1.0F / 6.0F;

    const float skew = (point.x + point.y + point.z) * kSkew;
    const Vec3 floored{std::floor(point.x + skew), std::floor(point.y + skew), std::floor(point.z + skew)};
    const float unskew = (floored.x + floored.y + floored.z) * kUnskew;
    const Vec3 first = point - (floored - Vec3{unskew, unskew, unskew});

    // The order of the offsets picks the tetrahedron, whose second and third corners step along the largest axes first.
    std::array<int, 3> second{};
    std::array<int, 3> third{};
    if (first.x >= first.y) {
        second = first.y >= first.z ? std::array<int, 3>{1, 0, 0} : (first.x >= first.z ? std::array<int, 3>{1, 0, 0} : std::array<int, 3>{0, 0, 1});
        third = first.y >= first.z ? std::array<int, 3>{1, 1, 0} : std::array<int, 3>{1, 0, 1};
    } else {
        second = first.y < first.z ? std::array<int, 3>{0, 0, 1} : (first.x < first.z ? std::array<int, 3>{0, 1, 0} : std::array<int, 3>{0, 1, 0});
        third = first.x < first.z ? std::array<int, 3>{0, 1, 1} : std::array<int, 3>{1, 1, 0};
    }

    const std::array<Vec3, 4> offsets{
        first,
        first - Vec3{static_cast<float>(second[0]), static_cast<float>(second[1]), static_cast<float>(second[2])} + Vec3{kUnskew, kUnskew, kUnskew},
        first - Vec3{static_cast<float>(third[0]), static_cast<float>(third[1]), static_cast<float>(third[2])} + Vec3{2.0F * kUnskew, 2.0F * kUnskew, 2.0F * kUnskew},
        first - Vec3{1.0F, 1.0F, 1.0F} + Vec3{3.0F * kUnskew, 3.0F * kUnskew, 3.0F * kUnskew},
    };
    const std::array<std::array<int, 3>, 4> steps{std::array<int, 3>{0, 0, 0}, second, third, std::array<int, 3>{1, 1, 1}};

    const int x = NoiseHelper::cellOf(floored.x);
    const int y = NoiseHelper::cellOf(floored.y);
    const int z = NoiseHelper::cellOf(floored.z);
    float total = 0.0F;
    for (std::size_t corner = 0; corner < offsets.size(); ++corner) {
        const Vec3 offset = offsets[corner];
        const float falloff = 0.6F - offset.getLengthSquared();
        if (falloff <= 0.0F) {
            continue;
        }

        const float squared = falloff * falloff;
        total += squared * squared * gradientDot(hash(x + steps[corner][0], y + steps[corner][1], z + steps[corner][2]), offset.x, offset.y, offset.z);
    }

    return std::clamp(total * 32.0F, -1.0F, 1.0F);
}

// In four dimensions the cell splits into 24 simplices, and the rank of each offset among the four tells how many of the four steps toward the far corner cross that axis.
float Noise3D::simplex(Vec4 point) const noexcept {
    const float kSkew = (std::sqrt(5.0F) - 1.0F) / 4.0F;
    const float kUnskew = (5.0F - std::sqrt(5.0F)) / 20.0F;

    const float skew = (point.x + point.y + point.z + point.w) * kSkew;
    const Vec4 floored{std::floor(point.x + skew), std::floor(point.y + skew), std::floor(point.z + skew), std::floor(point.w + skew)};
    const float unskew = (floored.x + floored.y + floored.z + floored.w) * kUnskew;
    const Vec4 first = point - (floored - Vec4{unskew, unskew, unskew, unskew});

    const std::array<float, 4> components{first.x, first.y, first.z, first.w};
    std::array<int, 4> rank{};
    for (std::size_t a = 0; a < 4; ++a) {
        for (std::size_t b = a + 1; b < 4; ++b) {
            ++rank[components[a] > components[b] ? a : b];
        }
    }

    const std::array<int, 4> cell{NoiseHelper::cellOf(floored.x), NoiseHelper::cellOf(floored.y), NoiseHelper::cellOf(floored.z), NoiseHelper::cellOf(floored.w)};
    float total = 0.0F;
    for (int corner = 0; corner <= 4; ++corner) {
        std::array<int, 4> step{};
        for (std::size_t axis = 0; axis < 4; ++axis) {
            step[axis] = rank[axis] >= 4 - corner ? 1 : 0;
        }

        const float shift = static_cast<float>(corner) * kUnskew;
        const Vec4 offset = first - Vec4{static_cast<float>(step[0]), static_cast<float>(step[1]), static_cast<float>(step[2]), static_cast<float>(step[3])} + Vec4{shift, shift, shift, shift};
        const float falloff = 0.6F - offset.getLengthSquared();
        if (falloff <= 0.0F) {
            continue;
        }

        const float squared = falloff * falloff;
        const std::uint8_t cornerHash = hash(cell[0] + step[0], cell[1] + step[1], cell[2] + step[2], cell[3] + step[3]);
        total += squared * squared * gradientDot(cornerHash, offset.x, offset.y, offset.z, offset.w);
    }

    return std::clamp(total * 27.0F, -1.0F, 1.0F);
}

float Noise3D::value(Vec3 point) const noexcept {
    const Vec3 floored{std::floor(point.x), std::floor(point.y), std::floor(point.z)};
    const int x = NoiseHelper::cellOf(floored.x);
    const int y = NoiseHelper::cellOf(floored.y);
    const int z = NoiseHelper::cellOf(floored.z);
    const Vec3 local = point - floored;
    const float u = NoiseHelper::fade(local.x);
    const float v = NoiseHelper::fade(local.y);
    const float w = NoiseHelper::fade(local.z);

    std::array<float, 8> corners{};
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
        corners[corner] = latticeValue(hash(x + static_cast<int>(corner & 1U), y + static_cast<int>((corner >> 1U) & 1U), z + static_cast<int>((corner >> 2U) & 1U)));
    }

    const float bottom = Math::lerp(Math::lerp(corners[0], corners[1], u), Math::lerp(corners[2], corners[3], u), v);
    const float top = Math::lerp(Math::lerp(corners[4], corners[5], u), Math::lerp(corners[6], corners[7], u), v);
    return Math::lerp(bottom, top, w);
}

float Noise3D::value(Vec4 point) const noexcept {
    const Vec4 floored{std::floor(point.x), std::floor(point.y), std::floor(point.z), std::floor(point.w)};
    const std::array<int, 4> cell{NoiseHelper::cellOf(floored.x), NoiseHelper::cellOf(floored.y), NoiseHelper::cellOf(floored.z), NoiseHelper::cellOf(floored.w)};
    const Vec4 local = point - floored;
    const std::array<float, 4> fades{NoiseHelper::fade(local.x), NoiseHelper::fade(local.y), NoiseHelper::fade(local.z), NoiseHelper::fade(local.w)};

    std::array<float, 16> corners{};
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
        corners[corner] = latticeValue(hash(cell[0] + static_cast<int>(corner & 1U), cell[1] + static_cast<int>((corner >> 1U) & 1U), cell[2] + static_cast<int>((corner >> 2U) & 1U), cell[3] + static_cast<int>((corner >> 3U) & 1U)));
    }

    std::size_t count = corners.size();
    for (const float fade : fades) {
        count /= 2;
        for (std::size_t index = 0; index < count; ++index) {
            corners[index] = Math::lerp(corners[index * 2], corners[index * 2 + 1], fade);
        }
    }

    return corners[0];
}

// Every cell holds one feature point, placed by three more hashes of the cell, and the nearest two of the cells around the point give the distances.
Noise3D::Cellular Noise3D::cellular(Vec3 point) const noexcept {
    const Vec3 floored{std::floor(point.x), std::floor(point.y), std::floor(point.z)};
    const int x = NoiseHelper::cellOf(floored.x);
    const int y = NoiseHelper::cellOf(floored.y);
    const int z = NoiseHelper::cellOf(floored.z);

    Cellular result{.nearest = 1e9F, .second = 1e9F};
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const std::uint8_t first = hash(x + dx, y + dy, z + dz);
                const std::uint8_t second = hash(x + dx + 101, y + dy + 57, z + dz + 13);
                const std::uint8_t third = hash(x + dx + 37, y + dy + 211, z + dz + 97);
                const Vec3 jitter{(static_cast<float>(first) + 0.5F) / 256.0F, (static_cast<float>(second) + 0.5F) / 256.0F, (static_cast<float>(third) + 0.5F) / 256.0F};
                const Vec3 feature = floored + Vec3{static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)} + jitter;
                const float distance = Vec3::distance(feature, point);
                if (distance < result.nearest) {
                    result.second = result.nearest;
                    result.nearest = distance;
                    result.cell = static_cast<float>(first * 256 + second) / 65536.0F;
                } else if (distance < result.second) {
                    result.second = distance;
                }
            }
        }
    }

    return result;
}

Noise3D::Cellular Noise3D::cellular(Vec4 point) const noexcept {
    const Vec4 floored{std::floor(point.x), std::floor(point.y), std::floor(point.z), std::floor(point.w)};
    const std::array<int, 4> cell{NoiseHelper::cellOf(floored.x), NoiseHelper::cellOf(floored.y), NoiseHelper::cellOf(floored.z), NoiseHelper::cellOf(floored.w)};

    Cellular result{.nearest = 1e9F, .second = 1e9F};
    for (int neighbor = 0; neighbor < 81; ++neighbor) {
        const std::array<int, 4> step{neighbor % 3 - 1, neighbor / 3 % 3 - 1, neighbor / 9 % 3 - 1, neighbor / 27 - 1};
        const std::array<int, 4> at{cell[0] + step[0], cell[1] + step[1], cell[2] + step[2], cell[3] + step[3]};
        const std::uint8_t first = hash(at[0], at[1], at[2], at[3]);
        const std::uint8_t second = hash(at[0] + 101, at[1] + 57, at[2] + 13, at[3] + 29);
        const std::uint8_t third = hash(at[0] + 37, at[1] + 211, at[2] + 97, at[3] + 61);
        const std::uint8_t fourth = hash(at[0] + 151, at[1] + 7, at[2] + 173, at[3] + 89);
        const Vec4 jitter{(static_cast<float>(first) + 0.5F) / 256.0F, (static_cast<float>(second) + 0.5F) / 256.0F, (static_cast<float>(third) + 0.5F) / 256.0F, (static_cast<float>(fourth) + 0.5F) / 256.0F};
        const Vec4 feature = floored + Vec4{static_cast<float>(step[0]), static_cast<float>(step[1]), static_cast<float>(step[2]), static_cast<float>(step[3])} + jitter;
        const float distance = Vec4::distance(feature, point);
        if (distance < result.nearest) {
            result.second = result.nearest;
            result.nearest = distance;
            result.cell = static_cast<float>(first * 256 + second) / 65536.0F;
        } else if (distance < result.second) {
            result.second = distance;
        }
    }

    return result;
}

float Noise3D::sampleBasis(Basis basis, Vec3 point) const noexcept {
    switch (basis) {
    case Basis::Perlin:
        return perlin(point);
    case Basis::Value:
        return value(point);
    case Basis::Simplex:
        break;
    }

    return simplex(point);
}

float Noise3D::sampleBasis(Basis basis, Vec4 point) const noexcept {
    switch (basis) {
    case Basis::Perlin:
        return perlin(point);
    case Basis::Value:
        return value(point);
    case Basis::Simplex:
        break;
    }

    return simplex(point);
}

// Billows and ridges fold the noise at its zero crossings, round side up for billows and sharp side up for ridges, and ping-pong bounces the stretched noise between the ends of the range.
float Noise3D::shape(Fractal fractal, float noise, float pingPongStrength) noexcept {
    switch (fractal) {
    case Fractal::Billow:
        return std::fabs(noise) * 2.0F - 1.0F;
    case Fractal::Ridged:
        return 1.0F - std::fabs(noise) * 2.0F;
    case Fractal::PingPong: {
        const float stretched = (noise + 1.0F) * pingPongStrength;
        const float folded = stretched - std::floor(stretched * 0.5F) * 2.0F;
        return (folded < 1.0F ? folded : 2.0F - folded) * 2.0F - 1.0F;
    }
    case Fractal::Fbm:
        break;
    }

    return noise;
}

template <typename Point> float Noise3D::sumOctaves(Point point, const FractalOptions& options) const {
    checkOctaves(options.octaves);
    float sum = 0.0F;
    float amplitude = 1.0F;
    float frequency = 1.0F;
    float normalization = 0.0F;
    for (int octave = 0; octave < options.octaves; ++octave) {
        sum += shape(options.fractal, sampleBasis(options.basis, point * frequency), options.pingPongStrength) * amplitude;
        normalization += amplitude;
        amplitude *= options.gain;
        frequency *= options.lacunarity;
    }

    return normalization > 0.0F ? sum / normalization : 0.0F;
}

float Noise3D::fractal(Vec3 point, const FractalOptions& options) const {
    return sumOctaves(point, options);
}

float Noise3D::fractal(Vec4 point, const FractalOptions& options) const {
    return sumOctaves(point, options);
}

Vec3 Noise3D::warp(Vec3 point, float amplitude, float frequency, int octaves) const {
    const FractalOptions options{.octaves = octaves};
    const Vec3 sampled = point * frequency;
    const float offsetX = fractal(sampled + Vec3{5.2F, 1.3F, 7.1F}, options);
    const float offsetY = fractal(sampled + Vec3{1.7F, 9.2F, 3.4F}, options);
    const float offsetZ = fractal(sampled + Vec3{8.3F, 2.8F, 6.6F}, options);
    return point + Vec3{offsetX, offsetY, offsetZ} * amplitude;
}

void Noise3D::checkOctaves(int octaves) {
    if (octaves < 1 || octaves > kMaxOctaves) {
        throw std::invalid_argument(std::format("The octaves must be a whole number from 1 to {}.", kMaxOctaves));
    }
}

} // namespace haylen::math
