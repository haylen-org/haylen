#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

#include "haylen/math/Color.hpp"
#include "haylen/math/ColorSpace.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Photometry.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

class ColorSpaceTest : public ::testing::Test {
  protected:
    static void expectColorNear(const Color& actual, const Color& expected, float tolerance) {
        EXPECT_NEAR(actual.r, expected.r, tolerance);
        EXPECT_NEAR(actual.g, expected.g, tolerance);
        EXPECT_NEAR(actual.b, expected.b, tolerance);
        EXPECT_NEAR(actual.a, expected.a, tolerance);
    }
};

// Every 8-bit value of sRGB decodes and encodes back to itself, so a color never takes the curve twice on the way.
TEST_F(ColorSpaceTest, SrgbRoundTripsEveryByteWithoutDoubleGamma) {
    for (int byte = 0; byte <= 255; ++byte) {
        const float encoded = static_cast<float>(byte) / 255.0F;
        const float linear = ColorSpace::decodeSrgb(encoded);
        EXPECT_NEAR(ColorSpace::encodeSrgb(linear), encoded, 1e-6F) << byte;
        EXPECT_LE(linear, encoded + 1e-7F) << byte;
    }

    EXPECT_NEAR(ColorSpace::decodeSrgb(0.5F), 0.21404F, 1e-5F);
    EXPECT_NEAR(ColorSpace::encodeSrgb(0.5F), 0.73536F, 1e-5F);
    EXPECT_EQ(ColorSpace::decodeSrgb(1.0F), 1.0F);
    EXPECT_EQ(ColorSpace::encodeSrgb(1.0F), 1.0F);
    EXPECT_EQ(ColorSpace::encodeSrgb(0.0F), 0.0F);
    EXPECT_EQ(Color::fromLinear(Color::white()), Color::white());
}

// HDR channels above 1 and the negative channels of wide colors keep their values, mirrored below 0 and continued above 1.
TEST_F(ColorSpaceTest, SrgbCurveExtendsBeyondTheRangeOfAScreen) {
    for (const float linear : {-0.5F, -0.001F, 1.5F, 4.0F, 100.0F}) {
        const float encoded = ColorSpace::encodeSrgb(linear);
        EXPECT_NEAR(ColorSpace::decodeSrgb(encoded), linear, std::fabs(linear) * 1e-5F) << linear;
        EXPECT_EQ(std::signbit(encoded), std::signbit(linear)) << linear;
    }

    EXPECT_GT(ColorSpace::encodeSrgb(2.0F), 1.0F);

    const Color hdr = Color{4.0F, 1.0F, 0.0F, 0.5F};
    expectColorNear(Color::fromLinear(hdr).getLinear(), hdr, 1e-5F);
    EXPECT_GT(Color::fromLinear(hdr).getLuminance(), 1.0F);
}

TEST_F(ColorSpaceTest, PerceptualQuantizerMatchesTheReferenceSignal) {
    EXPECT_NEAR(ColorSpace::encodePq(0.0F), 7.3e-7F, 1e-6F);
    EXPECT_NEAR(ColorSpace::encodePq(100.0F), 0.5081F, 1e-3F);
    EXPECT_NEAR(ColorSpace::encodePq(203.0F), 0.5806F, 1e-3F);
    EXPECT_NEAR(ColorSpace::encodePq(1000.0F), 0.7518F, 1e-3F);
    EXPECT_NEAR(ColorSpace::encodePq(10000.0F), 1.0F, 1e-6F);

    for (const float nits : {0.1F, 1.0F, 80.0F, 203.0F, 600.0F, 4000.0F}) {
        EXPECT_NEAR(ColorSpace::decodePq(ColorSpace::encodePq(nits)), nits, nits * 1e-3F) << nits;
    }

    EXPECT_EQ(ColorSpace::encodePq(-5.0F), ColorSpace::encodePq(0.0F));
    EXPECT_EQ(ColorSpace::encodePq(20000.0F), ColorSpace::encodePq(10000.0F));
}

// The white of every space is the white of D65 with a luminance of 1, and a color moves between primaries and back unchanged.
TEST_F(ColorSpaceTest, ConvertsBetweenPrimariesThroughXyz) {
    for (const ColorSpace::Kind space : {ColorSpace::Kind::Srgb, ColorSpace::Kind::DisplayP3, ColorSpace::Kind::Hdr10}) {
        const Vec3 white = ColorSpace::toXyz(Color::white(), space);
        EXPECT_NEAR(white.x, 0.95046F, 1e-4F);
        EXPECT_NEAR(white.y, 1.0F, 1e-4F);
        EXPECT_NEAR(white.z, 1.08906F, 1e-4F);
        EXPECT_NEAR(ColorSpace::luminance(Color::white(), space), 1.0F, 1e-4F);
    }

    const Color orange{0.9F, 0.4F, 0.1F, 0.25F};
    const Color inP3 = ColorSpace::convert(orange, ColorSpace::Kind::Srgb, ColorSpace::Kind::DisplayP3);
    expectColorNear(ColorSpace::convert(inP3, ColorSpace::Kind::DisplayP3, ColorSpace::Kind::Srgb), orange, 1e-5F);
    EXPECT_EQ(inP3.a, 0.25F);
    EXPECT_NEAR(ColorSpace::luminance(inP3, ColorSpace::Kind::DisplayP3), ColorSpace::luminance(orange, ColorSpace::Kind::Srgb), 1e-5F);

    // The pure red of Display P3 lies outside the gamut of sRGB, so its green and blue there fall below 0.
    const Color p3Red = ColorSpace::convert({1.0F, 0.0F, 0.0F, 1.0F}, ColorSpace::Kind::DisplayP3, ColorSpace::Kind::Srgb);
    EXPECT_GT(p3Red.r, 1.0F);
    EXPECT_LT(p3Red.g, 0.0F);
    EXPECT_LT(p3Red.b, 0.0F);
    expectColorNear(ColorSpace::fromXyz(ColorSpace::toXyz(orange, ColorSpace::Kind::Hdr10), ColorSpace::Kind::Hdr10), orange.withAlpha(1.0F), 1e-5F);
}

TEST_F(ColorSpaceTest, EncodesTheSignalOfEveryScreenAndBack) {
    const Color linear{0.5F, 0.2F, 2.0F, 0.75F};
    for (const ColorSpace::Kind output : {ColorSpace::Kind::Srgb, ColorSpace::Kind::DisplayP3, ColorSpace::Kind::Hdr10}) {
        const Color signal = ColorSpace::encode(linear, output);
        expectColorNear(ColorSpace::decode(signal, output), linear, 2e-3F);
        EXPECT_EQ(signal.a, 0.75F);
    }

    expectColorNear(ColorSpace::encode(Color::white(), ColorSpace::Kind::Srgb), Color::white(), 1e-5F);
    expectColorNear(ColorSpace::encode(Color::white(), ColorSpace::Kind::DisplayP3), Color::white(), 1e-4F);

    // The white of the scene reaches an HDR screen at the reference white, and a brighter reference white raises the signal.
    const Color hdrWhite = ColorSpace::encode(Color::white(), ColorSpace::Kind::Hdr10);
    EXPECT_NEAR(hdrWhite.g, ColorSpace::encodePq(ColorSpace::kReferenceWhite), 1e-4F);
    EXPECT_GT(ColorSpace::encode(Color::white(), ColorSpace::Kind::Hdr10, 400.0F).g, hdrWhite.g);
}

TEST_F(ColorSpaceTest, RefusesAReferenceWhiteOutsideTheRangeOfHdr) {
    for (const float referenceWhite : {0.0F, -1.0F, 20000.0F, std::nanf("")}) {
        EXPECT_THROW(static_cast<void>(ColorSpace::encode(Color::white(), ColorSpace::Kind::Hdr10, referenceWhite)), std::invalid_argument);
        EXPECT_THROW(static_cast<void>(ColorSpace::decode(Color::white(), ColorSpace::Kind::Hdr10, referenceWhite)), std::invalid_argument);
    }
}

TEST_F(ColorSpaceTest, NamesEverySpace) {
    for (const ColorSpace::Kind space : {ColorSpace::Kind::Srgb, ColorSpace::Kind::DisplayP3, ColorSpace::Kind::Hdr10}) {
        EXPECT_EQ(ColorSpace::kindFromName(ColorSpace::kindName(space)), space);
    }

    EXPECT_EQ(ColorSpace::kindName(ColorSpace::Kind::DisplayP3), "displayP3");
    EXPECT_FALSE(ColorSpace::kindFromName("rec2020").has_value());
}

// Warm light has more red than blue and cool light more blue than red, while the light of 6500 kelvin is close to white.
TEST_F(ColorSpaceTest, ColorTemperatureFollowsTheLightOfABlackBody) {
    const Color candle = Color::fromTemperature(1900.0F);
    EXPECT_EQ(candle.r, 1.0F);
    EXPECT_EQ(candle.b, 0.0F);
    EXPECT_NEAR(candle.g, 0.52F, 0.02F);

    const Color bulb = Color::fromTemperature(2700.0F);
    EXPECT_EQ(bulb.r, 1.0F);
    EXPECT_GT(bulb.g, bulb.b);
    EXPECT_NEAR(bulb.g, 0.68F, 0.02F);

    const Color daylight = Color::fromTemperature(6504.0F);
    expectColorNear(daylight, Color::white(), 0.03F);

    const Color sky = Color::fromTemperature(10000.0F);
    EXPECT_EQ(sky.b, 1.0F);
    EXPECT_LT(sky.r, sky.g);

    for (float kelvin = 1700.0F; kelvin < 25000.0F; kelvin += 700.0F) {
        const Color lower = Color::fromTemperature(kelvin).getLinear();
        const Color higher = Color::fromTemperature(kelvin + 100.0F).getLinear();
        EXPECT_GE(higher.b / higher.r, lower.b / lower.r) << kelvin;
    }

    EXPECT_THROW(static_cast<void>(Color::fromTemperature(1000.0F)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Color::fromTemperature(30000.0F)), std::invalid_argument);
}

TEST_F(ColorSpaceTest, LuminanceAndScalingKeepTheLightOfHdrColors) {
    EXPECT_NEAR(Color::fromHex(0x777777FFU).getLuminance(), 0.1845F, 0.001F);
    EXPECT_EQ((Color{0.5F, 1.0F, 2.0F, 0.5F} * 2.0F), (Color{1.0F, 2.0F, 4.0F, 0.5F}));

    const Color brighter = Color::fromLinear(Color::white() * 3.0F);
    EXPECT_NEAR(brighter.getLuminance(), 3.0F, 1e-4F);
}

TEST_F(ColorSpaceTest, PhotometryConvertsTheUnitsOfLight) {
    // A bulb of 800 lumens sends about 64 candelas in every direction, which give 64 lux one meter away and 16 lux at two meters.
    const float bulb = Photometry::pointIntensity(800.0F);
    EXPECT_NEAR(bulb, 63.662F, 1e-3F);
    EXPECT_NEAR(Photometry::illuminance(bulb, 2.0F), bulb / 4.0F, 1e-4F);

    // A spot whose cone opens half the sphere puts twice the candelas of a point light, and a narrower cone more.
    EXPECT_NEAR(Photometry::spotIntensity(800.0F, Math::kHalfPi), 2.0F * bulb, 1e-3F);
    EXPECT_NEAR(Photometry::spotIntensity(800.0F, Math::kPi), bulb, 1e-3F);
    EXPECT_GT(Photometry::spotIntensity(800.0F, 0.3F), Photometry::spotIntensity(800.0F, 0.6F));

    // The rule of sunny 16, f/16 at 1/100 of a second and ISO 100, is an exposure value near 15.
    EXPECT_NEAR(Photometry::ev100FromCamera(16.0F, 0.01F, 100.0F), 14.64F, 0.01F);
    EXPECT_NEAR(Photometry::ev100FromCamera(16.0F, 0.01F, 200.0F), 13.64F, 0.01F);
    EXPECT_NEAR(Photometry::ev100FromLuminance(0.125F), 0.0F, 1e-5F);
    EXPECT_NEAR(Photometry::ev100FromLuminance(4000.0F), 14.97F, 0.01F);
    EXPECT_NEAR(Photometry::exposure(0.0F), 1.0F / 1.2F, 1e-6F);
    EXPECT_NEAR(Photometry::exposure(1.0F), Photometry::exposure(0.0F) / 2.0F, 1e-6F);
}

TEST_F(ColorSpaceTest, PhotometryRefusesSettingsWithoutMeaning) {
    EXPECT_THROW(static_cast<void>(Photometry::spotIntensity(800.0F, 0.0F)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Photometry::spotIntensity(800.0F, 4.0F)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Photometry::illuminance(10.0F, 0.0F)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Photometry::ev100FromCamera(0.0F, 0.01F, 100.0F)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Photometry::ev100FromCamera(16.0F, -1.0F, 100.0F)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Photometry::ev100FromCamera(16.0F, 0.01F, 0.0F)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Photometry::ev100FromLuminance(0.0F)), std::invalid_argument);
}

} // namespace haylen::math
