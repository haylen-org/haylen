#include "haylen/math/SphericalHarmonics.hpp"

#include <cmath>
#include <cstdint>
#include <format>
#include <stdexcept>

#include "haylen/math/Math.hpp"

namespace haylen::math {

std::array<float, SphericalHarmonics::kCount> SphericalHarmonics::getBasis(Vec3 direction) {
    const Vec3 unit = direction.getNormalized();
    const float x = unit.x;
    const float y = unit.y;
    const float z = unit.z;
    return {0.282095F, 0.488603F * y, 0.488603F * z, 0.488603F * x, 1.092548F * x * y, 1.092548F * y * z, 0.315392F * (3.0F * z * z - 1.0F), 1.092548F * x * z, 0.546274F * (x * x - y * y)};
}

void SphericalHarmonics::checkSide(std::size_t side, const char* sentence) {
    if (side == 0 || side > kMaxSide) {
        throw std::invalid_argument(sentence);
    }
}

// The texel at `u` to the right and `v` down from the middle of a face, each from -1 to 1, looks along the axis of the face.
Vec3 SphericalHarmonics::getCubeDirection(int face, float u, float v) noexcept {
    switch (face) {
    case 0:
        return {1.0F, -v, -u};
    case 1:
        return {-1.0F, -v, u};
    case 2:
        return {u, 1.0F, v};
    case 3:
        return {u, -1.0F, -v};
    case 4:
        return {u, -v, 1.0F};
    default:
        return {-u, -v, -1.0F};
    }
}

// Each texel of a face covers the solid angle between the corners of its square on the unit cube, which the area of the projection of a corner gives exactly.
SphericalHarmonics SphericalHarmonics::fromCubemap(std::span<const float> rgb, std::size_t size) {
    checkSide(size, "The side of a cubemap must be from 1 to 16384 texels.");
    const auto expected = static_cast<std::uint64_t>(size) * size * 18U;
    if (rgb.size() != expected) {
        throw std::invalid_argument(std::format("A cubemap of side {} holds {} floats, the red, green and blue of each texel of its 6 faces, not {}.", size, expected, rgb.size()));
    }

    const auto corner = [](double x, double y) { return std::atan2(x * y, std::sqrt(x * x + y * y + 1.0)); };
    const double texel = 2.0 / static_cast<double>(size);
    SphericalHarmonics result;
    std::size_t first = 0;
    for (int face = 0; face < 6; ++face) {
        for (std::size_t row = 0; row < size; ++row) {
            const double top = -1.0 + texel * static_cast<double>(row);
            const auto v = static_cast<float>(top + texel / 2.0);
            for (std::size_t column = 0; column < size; ++column) {
                const double left = -1.0 + texel * static_cast<double>(column);
                const auto u = static_cast<float>(left + texel / 2.0);
                const double solidAngle = corner(left, top) - corner(left, top + texel) - corner(left + texel, top) + corner(left + texel, top + texel);
                result.projectTexel(getCubeDirection(face, u, v), rgb, first, static_cast<float>(solidAngle));
                first += 3;
            }
        }
    }

    return result;
}

// A texel of a panorama spans an even step of the azimuth and of the polar angle, so the solid angle it covers shrinks with the sine of its polar angle toward the poles.
SphericalHarmonics SphericalHarmonics::fromPanorama(std::span<const float> rgb, std::size_t width, std::size_t height) {
    checkSide(width, "The width and the height of a panorama must be from 1 to 16384 texels.");
    checkSide(height, "The width and the height of a panorama must be from 1 to 16384 texels.");
    const auto expected = static_cast<std::uint64_t>(width) * height * 3U;
    if (rgb.size() != expected) {
        throw std::invalid_argument(std::format("A panorama of {} by {} texels holds {} floats, the red, green and blue of each texel, not {}.", width, height, expected, rgb.size()));
    }

    const double across = Math::kTau / static_cast<double>(width);
    const double down = Math::kPi / static_cast<double>(height);
    SphericalHarmonics result;
    std::size_t first = 0;
    for (std::size_t row = 0; row < height; ++row) {
        const double polar = down * (static_cast<double>(row) + 0.5);
        const double sine = std::sin(polar);
        const auto solidAngle = static_cast<float>(across * down * sine);
        for (std::size_t column = 0; column < width; ++column) {
            const double azimuth = Math::kTau - across * (static_cast<double>(column) + 0.5);
            const Vec3 direction{static_cast<float>(sine * std::sin(azimuth)), static_cast<float>(std::cos(polar)), static_cast<float>(sine * std::cos(azimuth))};
            result.projectTexel(direction, rgb, first, solidAngle);
            first += 3;
        }
    }

    return result;
}

SphericalHarmonics SphericalHarmonics::lerp(const SphericalHarmonics& from, const SphericalHarmonics& to, float t) noexcept {
    SphericalHarmonics result;
    for (std::size_t index = 0; index < kCount; ++index) {
        result.coefficients[index] = Vec3::lerp(from.coefficients[index], to.coefficients[index], t);
    }

    return result;
}

void SphericalHarmonics::projectTexel(Vec3 direction, std::span<const float> rgb, std::size_t first, float weight) {
    const Vec3 light{rgb[first], rgb[first + 1], rgb[first + 2]};
    const std::array<float, kCount> basis = getBasis(direction);
    for (std::size_t index = 0; index < kCount; ++index) {
        coefficients[index] += light * (basis[index] * weight);
    }
}

void SphericalHarmonics::projectDirection(Vec3 direction, const Color& color, float weight) {
    const std::array<float, 3> rgb{color.r, color.g, color.b};
    projectTexel(direction, rgb, 0, weight);
}

Color SphericalHarmonics::evaluate(Vec3 direction) const {
    const std::array<float, kCount> basis = getBasis(direction);
    Vec3 light;
    for (std::size_t index = 0; index < kCount; ++index) {
        light += coefficients[index] * basis[index];
    }

    return {light.x, light.y, light.z, 1.0F};
}

// The Lambert law scales each band of the harmonics by its own factor, the convolution of a clamped cosine.
SphericalHarmonics SphericalHarmonics::convolveLambert() const noexcept {
    SphericalHarmonics result = *this;
    for (std::size_t index = 0; index < kCount; ++index) {
        const std::size_t band = index == 0 ? 0 : (index < 4 ? 1 : 2);
        result.coefficients[index] *= kLambertBands[band];
    }

    return result;
}

} // namespace haylen::math
