#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "haylen/math/Color.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// The light that arrives from every direction around a point, in the nine coefficients of the real spherical harmonics of the second order, such as the sky of a scene, a light probe or the light that an AR session estimates. Each coefficient holds red, green and blue of linear light in `x`, `y` and `z`. The basis functions of a unit direction are, in order, 0.282095, 0.488603 y, 0.488603 z, 0.488603 x, 1.092548 xy, 1.092548 yz, 0.315392 (3z² - 1), 1.092548 xz and 0.546274 (x² - y²).
class SphericalHarmonics final {
  public:
    static constexpr std::size_t kCount = 9;

    // The largest side of a cubemap or a panorama that the projections read, 16384 texels.
    static constexpr std::size_t kMaxSide = 16384;

    SphericalHarmonics() = default;
    explicit SphericalHarmonics(const std::array<Vec3, kCount>& values) noexcept : coefficients(values) {}

    // Returns the nine basis functions of a direction, which the method normalizes. Throws `std::invalid_argument` for a direction without a length.
    [[nodiscard]] static std::array<float, kCount> getBasis(Vec3 direction);

    // Projects the six faces of a cube of linear light, each `size` by `size` texels of red, green and blue, in the order positive x, negative x, positive y, negative y, positive z and negative z, with the rows of each face from the top, as graphics APIs lay out cube textures. Each texel counts with the solid angle it covers. Throws `std::invalid_argument` for a size of 0 or above `kMaxSide`, or for a count of floats other than 18 for each texel of a face.
    [[nodiscard]] static SphericalHarmonics fromCubemap(std::span<const float> rgb, std::size_t size);

    // Projects a panorama of linear light, `width` by `height` texels of red, green and blue from the top row down, whose rows run from straight up to straight down and whose middle column looks forward, along negative z, turning right toward its right edge. Throws `std::invalid_argument` for a side of 0 or above `kMaxSide`, or for a count of floats other than 3 for each texel.
    [[nodiscard]] static SphericalHarmonics fromPanorama(std::span<const float> rgb, std::size_t width, std::size_t height);

    [[nodiscard]] static SphericalHarmonics lerp(const SphericalHarmonics& from, const SphericalHarmonics& to, float t) noexcept;

    [[nodiscard]] const std::array<Vec3, kCount>& getCoefficients() const noexcept {
        return coefficients;
    }

    // Adds the light of `color` that arrives from a direction, scaled by `weight`, such as the solid angle that a sample covers or 1 for a light from one direction. Throws `std::invalid_argument` for a direction without a length.
    void projectDirection(Vec3 direction, const Color& color, float weight = 1.0F);

    // Returns the light that arrives from a direction, with an alpha of 1. Throws `std::invalid_argument` for a direction without a length.
    [[nodiscard]] Color evaluate(Vec3 direction) const;

    // Returns the harmonics of the irradiance, the light that a surface facing each direction receives from the whole half of the sphere in front of it, weighted by the cosine of the Lambert law. A matte white surface sends back its irradiance divided by pi.
    [[nodiscard]] SphericalHarmonics convolveLambert() const noexcept;

    [[nodiscard]] friend SphericalHarmonics operator+(const SphericalHarmonics& lhs, const SphericalHarmonics& rhs) noexcept {
        SphericalHarmonics sum = lhs;
        for (std::size_t index = 0; index < kCount; ++index) {
            sum.coefficients[index] += rhs.coefficients[index];
        }

        return sum;
    }

    [[nodiscard]] friend SphericalHarmonics operator*(const SphericalHarmonics& value, float scale) noexcept {
        SphericalHarmonics scaled = value;
        for (Vec3& coefficient : scaled.coefficients) {
            coefficient *= scale;
        }

        return scaled;
    }

    [[nodiscard]] friend bool operator==(const SphericalHarmonics&, const SphericalHarmonics&) noexcept = default;

  private:
    // The Lambert law over the bands of order 0, 1 and 2.
    static constexpr std::array<float, 3> kLambertBands{Math::kPi, Math::kTau / 3.0F, Math::kPi / 4.0F};

    // Returns the direction of a texel of a face of a cube in the order of `fromCubemap`.
    [[nodiscard]] static Vec3 getCubeDirection(int face, float u, float v) noexcept;

    // Throws `std::invalid_argument` for a side of 0 or above `kMaxSide`.
    static void checkSide(std::size_t side, const char* sentence);

    // Adds a texel whose red, green and blue start at `first`.
    void projectTexel(Vec3 direction, std::span<const float> rgb, std::size_t first, float weight);

    std::array<Vec3, kCount> coefficients{};
};

} // namespace haylen::math
