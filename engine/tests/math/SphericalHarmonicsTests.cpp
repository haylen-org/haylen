#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "haylen/math/Color.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/SphericalHarmonics.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

class SphericalHarmonicsTest : public ::testing::Test {
  protected:
    // The red, green and blue of a sky of one color in every texel.
    static std::vector<float> fill(std::size_t texels, const Color& color) {
        std::vector<float> rgb;
        for (std::size_t texel = 0; texel < texels; ++texel) {
            rgb.insert(rgb.end(), {color.r, color.g, color.b});
        }

        return rgb;
    }

    static void expectColor(const Color& actual, const Color& expected, float tolerance) {
        EXPECT_NEAR(actual.r, expected.r, tolerance);
        EXPECT_NEAR(actual.g, expected.g, tolerance);
        EXPECT_NEAR(actual.b, expected.b, tolerance);
    }

    // A sky of one color projects into the first coefficient alone, which is the color times the square root of 4 pi, and its irradiance is pi times the color from every direction.
    static void expectUniformSky(const SphericalHarmonics& sky, const Color& color, float tolerance) {
        const std::array<Vec3, SphericalHarmonics::kCount>& coefficients = sky.getCoefficients();
        EXPECT_NEAR(coefficients[0].x, color.r * 3.5449077F, tolerance);
        EXPECT_NEAR(coefficients[0].z, color.b * 3.5449077F, tolerance);
        for (std::size_t index = 1; index < SphericalHarmonics::kCount; ++index) {
            EXPECT_NEAR(coefficients[index].x, 0.0F, tolerance) << index;
            EXPECT_NEAR(coefficients[index].y, 0.0F, tolerance) << index;
        }

        const SphericalHarmonics irradiance = sky.convolveLambert();
        for (const Vec3 direction : {Vec3::up(), Vec3::forward(), Vec3{1.0F, -1.0F, 0.5F}}) {
            expectColor(irradiance.evaluate(direction), {color.r * Math::kPi, color.g * Math::kPi, color.b * Math::kPi}, tolerance * 4.0F);
        }
    }
};

// The light of one direction projects into the basis of that direction, and its irradiance follows the known values of the second order: 1.0625 facing the light and 0.0625 facing away.
TEST_F(SphericalHarmonicsTest, ProjectsTheLightOfOneDirection) {
    SphericalHarmonics sun;
    sun.projectDirection({0.0F, 2.0F, 0.0F}, {2.0F, 1.0F, 0.5F});
    const std::array<Vec3, SphericalHarmonics::kCount>& coefficients = sun.getCoefficients();
    const std::array<float, SphericalHarmonics::kCount> expected{0.282095F, 0.488603F, 0.0F, 0.0F, 0.0F, 0.0F, -0.315392F, 0.0F, -0.546274F};
    for (std::size_t index = 0; index < SphericalHarmonics::kCount; ++index) {
        EXPECT_NEAR(coefficients[index].x, expected[index] * 2.0F, 1e-6F) << index;
        EXPECT_NEAR(coefficients[index].y, expected[index], 1e-6F) << index;
        EXPECT_NEAR(coefficients[index].z, expected[index] * 0.5F, 1e-6F) << index;
    }

    const SphericalHarmonics irradiance = sun.convolveLambert();
    expectColor(irradiance.evaluate(Vec3::up()), {2.125F, 1.0625F, 0.53125F}, 1e-4F);
    expectColor(irradiance.evaluate(-Vec3::up()), {0.125F, 0.0625F, 0.03125F}, 1e-4F);
    EXPECT_EQ(irradiance.evaluate(Vec3::up()).a, 1.0F);
    EXPECT_THROW(static_cast<void>(sun.evaluate({})), std::invalid_argument);
}

TEST_F(SphericalHarmonicsTest, ProjectsSkiesOfCubemapsAndPanoramas) {
    const Color sky{0.4F, 0.6F, 1.2F, 1.0F};
    expectUniformSky(SphericalHarmonics::fromCubemap(fill(6 * 16 * 16, sky), 16), sky, 1e-4F);
    expectUniformSky(SphericalHarmonics::fromPanorama(fill(64 * 32, sky), 64, 32), sky, 5e-3F);

    // A cubemap lit only on its face toward positive x and a panorama lit only around its middle column bring the most light from the right and from the front.
    std::vector<float> right = fill(6 * 8 * 8, {0.0F, 0.0F, 0.0F, 1.0F});
    std::fill(right.begin(), right.begin() + 8 * 8 * 3, 1.0F);
    const SphericalHarmonics fromRight = SphericalHarmonics::fromCubemap(right, 8);
    EXPECT_GT(fromRight.evaluate({1.0F, 0.0F, 0.0F}).r, fromRight.evaluate({0.0F, 0.0F, 1.0F}).r);
    EXPECT_GT(fromRight.evaluate({1.0F, 0.0F, 0.0F}).r, fromRight.evaluate({-1.0F, 0.0F, 0.0F}).r);

    std::vector<float> front = fill(16 * 8, {0.0F, 0.0F, 0.0F, 1.0F});
    for (std::size_t row = 0; row < 8; ++row) {
        std::fill_n(front.begin() + static_cast<std::ptrdiff_t>((row * 16 + 7) * 3), 6, 1.0F);
    }

    const SphericalHarmonics fromFront = SphericalHarmonics::fromPanorama(front, 16, 8);
    EXPECT_GT(fromFront.evaluate(Vec3::forward()).r, fromFront.evaluate(-Vec3::forward()).r);
    EXPECT_GT(fromFront.evaluate(Vec3::forward()).r, fromFront.evaluate(Vec3::right()).r);

    EXPECT_THROW(static_cast<void>(SphericalHarmonics::fromCubemap(fill(5, sky), 1)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(SphericalHarmonics::fromCubemap({}, 0)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(SphericalHarmonics::fromPanorama(fill(4, sky), 2, 3)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(SphericalHarmonics::fromPanorama({}, 0, 1)), std::invalid_argument);
}

TEST_F(SphericalHarmonicsTest, AddsScalesAndBlends) {
    SphericalHarmonics warm;
    warm.projectDirection(Vec3::right(), {1.0F, 0.5F, 0.0F});
    SphericalHarmonics cold;
    cold.projectDirection(-Vec3::right(), {0.0F, 0.5F, 1.0F});

    const Color both = (warm + cold).evaluate(Vec3::up());
    const Color sum{warm.evaluate(Vec3::up()).r + cold.evaluate(Vec3::up()).r, warm.evaluate(Vec3::up()).g + cold.evaluate(Vec3::up()).g, warm.evaluate(Vec3::up()).b + cold.evaluate(Vec3::up()).b, 1.0F};
    expectColor(both, sum, 1e-6F);
    expectColor((warm * 3.0F).evaluate(Vec3::right()), {warm.evaluate(Vec3::right()).r * 3.0F, warm.evaluate(Vec3::right()).g * 3.0F, 0.0F, 1.0F}, 1e-5F);
    EXPECT_EQ(SphericalHarmonics::lerp(warm, cold, 0.0F), warm);
    EXPECT_EQ(SphericalHarmonics::lerp(warm, cold, 1.0F), cold);
    expectColor(SphericalHarmonics::lerp(warm, cold, 0.5F).evaluate(Vec3::forward()), ((warm + cold) * 0.5F).evaluate(Vec3::forward()), 1e-6F);
}

} // namespace haylen::math
