#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "haylen/graphics/Gradient.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::graphics {

class GradientTest : public ::testing::Test {
  protected:
    // A gradient from black to white without dithering, whose pixels read back as the position they were painted at.
    [[nodiscard]] static Gradient plain(Gradient::Kind kind) {
        Gradient gradient({{0.0F, haylen::math::Color::black()}, {1.0F, haylen::math::Color::white()}}, kind);
        gradient.setDithered(false);
        return gradient;
    }

    [[nodiscard]] static int red(const Image& image, int x, int y) {
        return image.getPixels()[static_cast<std::size_t>(y * image.getWidth() + x) * 4U];
    }
};

// Colors blend premultiplied between the stops around a position, hold the first and last colors past them, and switch at a hard stop.
TEST_F(GradientTest, BlendsTheColorsOfTheStopsAroundAPosition) {
    const Gradient gradient({{0.25F, haylen::math::Color::fromHex(0xFF0000FFU)}, {0.5F, haylen::math::Color::fromHex(0x0000FFFFU)}, {0.5F, haylen::math::Color::fromHex(0x00FF00FFU)}, {1.0F, haylen::math::Color::fromHex(0x00FF0000U)}});
    EXPECT_EQ(gradient.getColor(0.0F), haylen::math::Color::fromHex(0xFF0000FFU));
    EXPECT_EQ(gradient.getColor(0.375F).toHex(), "#FF800080");
    EXPECT_EQ(gradient.getColor(0.5F), haylen::math::Color::fromHex(0x00FF00FFU));
    EXPECT_EQ(gradient.getColor(2.0F), haylen::math::Color::fromHex(0x00FF0000U));

    // A stop that fades out keeps the color of its neighbour instead of blending toward black.
    const haylen::math::Color fading = gradient.getColor(0.75F);
    EXPECT_NEAR(fading.a, 0.5F, 0.0001F);
    EXPECT_NEAR(fading.g, 1.0F, 0.0001F);
    EXPECT_EQ(Gradient::fromColors(std::vector{haylen::math::Color::black(), haylen::math::Color::white(), haylen::math::Color::black()}).getStops()[1].offset, 0.5F);

    EXPECT_THROW(Gradient({}), std::invalid_argument);
    EXPECT_THROW(Gradient({{0.5F, haylen::math::Color::white()}, {0.25F, haylen::math::Color::black()}}), std::invalid_argument);
    EXPECT_THROW(Gradient({{1.5F, haylen::math::Color::white()}}), std::invalid_argument);
    Gradient radial = plain(Gradient::Kind::Radial);
    EXPECT_THROW(radial.setRadius({0.0F, 1.0F}), std::invalid_argument);
}

// A linear gradient spans the area from the corner behind its direction to the corner ahead of it, at the centers of the pixels.
TEST_F(GradientTest, PaintsALinearGradientAcrossTheArea) {
    Image row(4, 1);
    plain(Gradient::Kind::Linear).paint(row, row.getArea());
    EXPECT_EQ(red(row, 0, 0), 32);
    EXPECT_EQ(red(row, 1, 0), 96);
    EXPECT_EQ(red(row, 2, 0), 159);
    EXPECT_EQ(red(row, 3, 0), 223);

    Gradient upward = plain(Gradient::Kind::Linear);
    upward.setAngle(-haylen::math::Math::kHalfPi);
    Image column(1, 4);
    upward.paint(column, column.getArea());
    EXPECT_EQ(red(column, 0, 0), 223);
    EXPECT_EQ(red(column, 0, 3), 32);

    // The area is the frame of the gradient, and only its pixels inside the image change.
    Image framed(4, 2, haylen::math::Color::fromHex(0x102030FFU));
    plain(Gradient::Kind::Linear).paint(framed, {2, 0, 4, 1});
    EXPECT_EQ(red(framed, 1, 0), 0x10);
    EXPECT_EQ(red(framed, 2, 0), 32);
    EXPECT_EQ(red(framed, 3, 0), 96);
    EXPECT_EQ(red(framed, 2, 1), 0x10);
}

// A radial gradient grows from its center to its radius, a share of each side, and a conic one turns clockwise from its angle.
TEST_F(GradientTest, PaintsRadialAndConicGradientsAroundTheirCenter) {
    Image disc(5, 5);
    plain(Gradient::Kind::Radial).paint(disc, disc.getArea());
    EXPECT_EQ(red(disc, 2, 2), 0);
    EXPECT_EQ(red(disc, 4, 2), 204);
    EXPECT_EQ(red(disc, 0, 0), 255);

    Gradient offCenter = plain(Gradient::Kind::Radial);
    offCenter.setCenter({0.0F, 0.5F});
    offCenter.setRadius({1.0F, 1.0F});
    offCenter.paint(disc, disc.getArea());
    EXPECT_EQ(red(disc, 0, 2), 26);
    EXPECT_EQ(red(disc, 4, 2), 230);

    Image dial(9, 9);
    plain(Gradient::Kind::Conic).paint(dial, dial.getArea());
    EXPECT_NEAR(red(dial, 8, 4), 0, 1);
    EXPECT_NEAR(red(dial, 4, 8), 64, 1);
    EXPECT_NEAR(red(dial, 0, 4), 128, 1);
    EXPECT_NEAR(red(dial, 4, 0), 191, 1);
}

// Dithering spreads the rounding of a color between two steps over a pattern, so its average stays the color without bands.
TEST_F(GradientTest, DithersTheRoundingOfEveryPixel) {
    Image gray(4, 4);
    Gradient::fromColors(std::vector{haylen::math::Color(127.5F / 255.0F, 0.0F, 0.0F, 1.0F)}).paint(gray, gray.getArea());
    int sum = 0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            EXPECT_TRUE(red(gray, x, y) == 127 || red(gray, x, y) == 128);
            sum += red(gray, x, y);
        }
    }

    EXPECT_EQ(sum, 127 * 8 + 128 * 8);
}

} // namespace haylen::graphics
