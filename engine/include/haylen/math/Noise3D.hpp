#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Vec3.hpp"
#include "haylen/math/Vec4.hpp"

namespace haylen::math {

// Seeded noise in three and four dimensions for procedural content, such as terrain, caves, clouds and textures. The fourth dimension often runs with time, so a 3D pattern changes smoothly without sliding. Gradient and value noises are in [-1, 1], and the same seed gives the same values on every platform.
class Noise3D final {
  public:
    // The noise that each octave of a fractal sums.
    enum class Basis : std::uint8_t {
        Perlin,
        Simplex,
        Value,
    };

    // How each octave shapes its noise before the sum: as it is, folded into round billows, folded into sharp ridges, or bounced back and forth between -1 and 1.
    enum class Fractal : std::uint8_t {
        Fbm,
        Billow,
        Ridged,
        PingPong,
    };

    struct FractalOptions {
        Basis basis = Basis::Simplex;
        Fractal fractal = Fractal::Fbm;
        int octaves = 4;
        float lacunarity = 2.0F;
        float gain = 0.5F;
        float pingPongStrength = 2.0F;
    };

    // Distances run in cell units from the nearest and second nearest feature points, and `cell` tells the region of the nearest point apart with a value in [0, 1).
    struct Cellular {
        float nearest = 0.0F;
        float second = 0.0F;
        float cell = 0.0F;
    };

    // The most octaves a fractal sums, since each one samples the noise once more.
    static constexpr int kMaxOctaves = 16;

    explicit Noise3D(std::uint64_t seed = 0) noexcept;

    [[nodiscard]] static std::optional<Basis> basisFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view basisName(Basis value) noexcept;
    [[nodiscard]] static std::optional<Fractal> fractalFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view fractalName(Fractal value) noexcept;

    [[nodiscard]] float perlin(Vec3 point) const noexcept;
    [[nodiscard]] float perlin(Vec4 point) const noexcept;
    [[nodiscard]] float simplex(Vec3 point) const noexcept;
    [[nodiscard]] float simplex(Vec4 point) const noexcept;
    [[nodiscard]] float value(Vec3 point) const noexcept;
    [[nodiscard]] float value(Vec4 point) const noexcept;

    // Worley noise with one jittered feature point per unit cell.
    [[nodiscard]] Cellular cellular(Vec3 point) const noexcept;
    [[nodiscard]] Cellular cellular(Vec4 point) const noexcept;

    // Sums octaves of the basis, each at the frequency of the one before times the lacunarity and the amplitude times the gain, scaled back into [-1, 1]. Throws `std::invalid_argument` for octaves outside 1 to `kMaxOctaves`.
    [[nodiscard]] float fractal(Vec3 point, const FractalOptions& options) const;
    [[nodiscard]] float fractal(Vec4 point, const FractalOptions& options) const;

    // Moves the point by fractal simplex noise sampled at `frequency`, up to `amplitude` away on each axis, so sampling any noise at the result bends its patterns. Throws `std::invalid_argument` for octaves outside 1 to `kMaxOctaves`.
    [[nodiscard]] Vec3 warp(Vec3 point, float amplitude, float frequency = 1.0F, int octaves = 3) const;

  private:
    static const std::array<std::pair<std::string_view, Basis>, 3> kBasisNames;
    static const std::array<std::pair<std::string_view, Fractal>, 4> kFractalNames;

    // The twelve edges of a cube, the gradients of 3D noise.
    static constexpr std::array<std::array<float, 3>, 12> kGradients3{{
        {1.0F, 1.0F, 0.0F},
        {-1.0F, 1.0F, 0.0F},
        {1.0F, -1.0F, 0.0F},
        {-1.0F, -1.0F, 0.0F},
        {1.0F, 0.0F, 1.0F},
        {-1.0F, 0.0F, 1.0F},
        {1.0F, 0.0F, -1.0F},
        {-1.0F, 0.0F, -1.0F},
        {0.0F, 1.0F, 1.0F},
        {0.0F, -1.0F, 1.0F},
        {0.0F, 1.0F, -1.0F},
        {0.0F, -1.0F, -1.0F},
    }};

    // The 32 edges of a 4D cube, the gradients of 4D noise.
    static constexpr std::array<std::array<float, 4>, 32> kGradients4{{
        {0.0F, 1.0F, 1.0F, 1.0F}, {0.0F, 1.0F, 1.0F, -1.0F}, {0.0F, 1.0F, -1.0F, 1.0F}, {0.0F, 1.0F, -1.0F, -1.0F}, {0.0F, -1.0F, 1.0F, 1.0F}, {0.0F, -1.0F, 1.0F, -1.0F}, {0.0F, -1.0F, -1.0F, 1.0F}, {0.0F, -1.0F, -1.0F, -1.0F}, {1.0F, 0.0F, 1.0F, 1.0F}, {1.0F, 0.0F, 1.0F, -1.0F}, {1.0F, 0.0F, -1.0F, 1.0F}, {1.0F, 0.0F, -1.0F, -1.0F}, {-1.0F, 0.0F, 1.0F, 1.0F}, {-1.0F, 0.0F, 1.0F, -1.0F}, {-1.0F, 0.0F, -1.0F, 1.0F}, {-1.0F, 0.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 0.0F, 1.0F}, {1.0F, 1.0F, 0.0F, -1.0F}, {1.0F, -1.0F, 0.0F, 1.0F}, {1.0F, -1.0F, 0.0F, -1.0F}, {-1.0F, 1.0F, 0.0F, 1.0F}, {-1.0F, 1.0F, 0.0F, -1.0F}, {-1.0F, -1.0F, 0.0F, 1.0F}, {-1.0F, -1.0F, 0.0F, -1.0F}, {1.0F, 1.0F, 1.0F, 0.0F}, {1.0F, 1.0F, -1.0F, 0.0F}, {1.0F, -1.0F, 1.0F, 0.0F}, {1.0F, -1.0F, -1.0F, 0.0F}, {-1.0F, 1.0F, 1.0F, 0.0F}, {-1.0F, 1.0F, -1.0F, 0.0F}, {-1.0F, -1.0F, 1.0F, 0.0F}, {-1.0F, -1.0F, -1.0F, 0.0F},
    }};

    [[nodiscard]] std::uint8_t hash(int x, int y, int z) const noexcept;
    [[nodiscard]] std::uint8_t hash(int x, int y, int z, int w) const noexcept;
    [[nodiscard]] static float gradientDot(std::uint8_t cornerHash, float x, float y, float z) noexcept;
    [[nodiscard]] static float gradientDot(std::uint8_t cornerHash, float x, float y, float z, float w) noexcept;
    [[nodiscard]] static float latticeValue(std::uint8_t cornerHash) noexcept;
    [[nodiscard]] float sampleBasis(Basis basis, Vec3 point) const noexcept;
    [[nodiscard]] float sampleBasis(Basis basis, Vec4 point) const noexcept;
    template <typename Point> [[nodiscard]] float sumOctaves(Point point, const FractalOptions& options) const;
    [[nodiscard]] static float shape(Fractal fractal, float noise, float pingPongStrength) noexcept;
    static void checkOctaves(int octaves);

    std::array<std::uint8_t, 512> permutation{};
};

} // namespace haylen::math
