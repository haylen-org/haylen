#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Color.hpp"
#include "haylen/math/Mat3.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// The color spaces of the screens that the engine draws for, with the conversions between linear light, the primaries of each space and the signal that each screen takes. A color of linear light holds amounts of light that add up, and its channels go above 1 for light brighter than the white of the screen.
class ColorSpace final {
  public:
    enum class Kind : std::uint8_t {
        Srgb,
        DisplayP3,
        Hdr10,
    };

    // The nits that the white of a color with channels of 1 takes on an HDR screen, the reference white of graphics in HDR video.
    static constexpr float kReferenceWhite = 203.0F;
    // The brightest light that the signal of HDR10 carries, in nits.
    static constexpr float kPeakNits = 10000.0F;

    [[nodiscard]] static std::optional<Kind> kindFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view kindName(Kind value) noexcept;

    // The transfer curve of sRGB, which Display P3 shares, from linear light to the encoded value and back. It mirrors below 0 and continues above 1, so the channels of HDR and wide colors keep their values through a round trip.
    [[nodiscard]] static float encodeSrgb(float linear) noexcept;
    [[nodiscard]] static float decodeSrgb(float encoded) noexcept;

    // The perceptual quantizer of HDR10, from light in nits to the signal from 0 to 1 and back. Light below 0 or above `kPeakNits` takes the nearest end.
    [[nodiscard]] static float encodePq(float nits) noexcept;
    [[nodiscard]] static float decodePq(float signal) noexcept;

    // Moves a color of linear light from the primaries of one space to those of another, keeping its alpha. HDR10 has the primaries of Rec. 2020. A color outside the gamut of the space it moves to has channels below 0 or above 1.
    [[nodiscard]] static Color convert(const Color& linear, Kind from, Kind to) noexcept;

    // The CIE XYZ coordinates of a color of linear light in a space and back, where `y` is the luminance and the white of every space has `y` of 1.
    [[nodiscard]] static Vec3 toXyz(const Color& linear, Kind space) noexcept;
    [[nodiscard]] static Color fromXyz(Vec3 xyz, Kind space) noexcept;

    [[nodiscard]] static float luminance(const Color& linear, Kind space) noexcept;

    // Turns a color of linear light with the primaries of sRGB, the light that a scene draws, into the signal of a screen: sRGB and Display P3 encode it with the curve of sRGB, and HDR10 places its white at `referenceWhite` nits and encodes it with the perceptual quantizer. Alpha stays as it is. Throws `std::invalid_argument` for a reference white that is not above 0 and at most `kPeakNits`.
    [[nodiscard]] static Color encode(const Color& linear, Kind output, float referenceWhite = kReferenceWhite);
    // Turns the signal of a screen back into a color of linear light with the primaries of sRGB, undoing `encode`.
    [[nodiscard]] static Color decode(const Color& signal, Kind output, float referenceWhite = kReferenceWhite);

  private:
    static const std::array<std::pair<std::string_view, Kind>, 3> kKindNames;

    // The constants of the curve of SMPTE ST 2084, which works on light as a share of `kPeakNits`.
    static constexpr float kPqM1 = 0.1593017578125F;
    static constexpr float kPqM2 = 78.84375F;
    static constexpr float kPqC1 = 0.8359375F;
    static constexpr float kPqC2 = 18.8515625F;
    static constexpr float kPqC3 = 18.6875F;

    // The matrices from the linear primaries of each space to CIE XYZ with the white of D65, by columns, and their inverses.
    static constexpr Mat3 kSrgbToXyz = Mat3::fromColumns({0.4123908F, 0.2126390F, 0.0193308F}, {0.3575843F, 0.7151687F, 0.1191948F}, {0.1804808F, 0.0721923F, 0.9505322F});
    static constexpr Mat3 kXyzToSrgb = Mat3::fromColumns({3.2409699F, -0.9692436F, 0.0556301F}, {-1.5373832F, 1.8759675F, -0.2039770F}, {-0.4986108F, 0.0415551F, 1.0569715F});
    static constexpr Mat3 kDisplayP3ToXyz = Mat3::fromColumns({0.4865709F, 0.2289746F, 0.0F}, {0.2656677F, 0.6917385F, 0.0451134F}, {0.1982173F, 0.0792869F, 1.0439444F});
    static constexpr Mat3 kXyzToDisplayP3 = Mat3::fromColumns({2.4934969F, -0.8294890F, 0.0358458F}, {-0.9313836F, 1.7626641F, -0.0761724F}, {-0.4027108F, 0.0236247F, 0.9568845F});
    static constexpr Mat3 kRec2020ToXyz = Mat3::fromColumns({0.6369580F, 0.2627002F, 0.0F}, {0.1446169F, 0.6779981F, 0.0280727F}, {0.1688810F, 0.0593017F, 1.0609851F});
    static constexpr Mat3 kXyzToRec2020 = Mat3::fromColumns({1.7166512F, -0.6666844F, 0.0176399F}, {-0.3556708F, 1.6164812F, -0.0427706F}, {-0.2533663F, 0.0157685F, 0.9421031F});

    [[nodiscard]] static const Mat3& getToXyz(Kind space) noexcept;
    [[nodiscard]] static const Mat3& getFromXyz(Kind space) noexcept;
    static void checkReferenceWhite(float referenceWhite);
};

} // namespace haylen::math
