#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>

#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/ImageFilters.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::graphics {

class ImageFiltersTest : public ::testing::Test {
  protected:
    [[nodiscard]] static Image makeFlat(int width, int height, std::uint32_t color) {
        return Image(width, height, haylen::math::Color::fromHex(color));
    }
};

// A blur spreads a block over its surroundings, and since it mixes premultiplied colors the transparent pixels around a red block never turn its edge dark.
TEST_F(ImageFiltersTest, BlursWithoutDarkeningTransparentEdges) {
    Image image(21, 21);
    image.fill({8, 8, 5, 5}, haylen::math::Color::fromHex(0xFF0000FFU));
    ImageFilters::blur(image, 2.0F);
    const haylen::math::Color center = image.getPixel(10, 10);
    const haylen::math::Color edge = image.getPixel(6, 10);
    EXPECT_LT(center.a, 1.0F);
    EXPECT_GT(center.a, 0.5F);
    EXPECT_GT(edge.a, 0.0F);
    EXPECT_LT(edge.a, center.a);
    EXPECT_EQ(edge.r, 1.0F);
    EXPECT_EQ(edge.g, 0.0F);
    EXPECT_EQ(image.getPixel(0, 0).a, 0.0F);

    Image flat = makeFlat(8, 8, 0x336699FFU);
    ImageFilters::blur(flat, 3.0F);
    EXPECT_EQ(flat.getPixel(4, 4).toHex(), "#FF336699");
    EXPECT_THROW(ImageFilters::blur(flat, -1.0F), std::invalid_argument);
    EXPECT_THROW(ImageFilters::blur(flat, ImageFilters::kMaxRadius + 1.0F), std::invalid_argument);
}

TEST_F(ImageFiltersTest, SharpensTheEdgesBetweenColors) {
    Image image = makeFlat(10, 4, 0x808080FFU);
    image.fill({5, 0, 5, 4}, haylen::math::Color::fromHex(0x404040FFU));
    ImageFilters::sharpen(image, 1.0F);
    EXPECT_GT(image.getPixel(4, 2).r, 0.502F);
    EXPECT_LT(image.getPixel(5, 2).r, 0.25F);
    EXPECT_NEAR(image.getPixel(0, 2).r, 0.502F, 0.004F);
    EXPECT_THROW(ImageFilters::sharpen(image, -1.0F), std::invalid_argument);
}

TEST_F(ImageFiltersTest, AdjustsBrightnessContrastSaturationAndHue) {
    Image gray = makeFlat(1, 1, 0x808080FFU);
    ImageFilters::adjust(gray, {.brightness = 0.1F});
    EXPECT_NEAR(gray.getPixel(0, 0).r, 0.6F, 0.004F);

    Image light = makeFlat(1, 1, 0xBFBFBFFFU);
    ImageFilters::adjust(light, {.contrast = 2.0F});
    EXPECT_NEAR(light.getPixel(0, 0).r, 1.0F, 0.004F);

    Image red = makeFlat(1, 1, 0xFF0000FFU);
    ImageFilters::adjust(red, {.hue = 1.0F / 3.0F});
    EXPECT_EQ(red.getPixel(0, 0).toHex(), "#FF00FF00");

    Image vivid = makeFlat(1, 1, 0xFF0000FFU);
    ImageFilters::grayscale(vivid, 1.0F);
    const haylen::math::Color luma = vivid.getPixel(0, 0);
    EXPECT_NEAR(luma.r, 0.2126F, 0.004F);
    EXPECT_EQ(luma.r, luma.g);
    EXPECT_EQ(luma.g, luma.b);
}

TEST_F(ImageFiltersTest, InvertsTintsAndPixelates) {
    Image image = makeFlat(1, 1, 0x0A141E80U);
    ImageFilters::invert(image);
    EXPECT_EQ(image.getPixel(0, 0).toHex(), "#80F5EBE1");

    Image red = makeFlat(1, 1, 0xFF0000FFU);
    ImageFilters::tint(red, haylen::math::Color::fromHex(0x0000FFFFU), 0.5F);
    EXPECT_EQ(red.getPixel(0, 0).toHex(), "#FF800080");

    Image checker(2, 2);
    checker.setPixel(0, 0, haylen::math::Color::fromHex(0xFF0000FFU));
    checker.setPixel(1, 0, haylen::math::Color::fromHex(0x00FF00FFU));
    checker.setPixel(0, 1, haylen::math::Color::fromHex(0x0000FFFFU));
    checker.setPixel(1, 1, haylen::math::Color::fromHex(0xFFFFFFFFU));
    ImageFilters::pixelate(checker, 2);
    EXPECT_EQ(checker.getPixel(0, 0).toHex(), "#FF808080");
    EXPECT_EQ(checker.getPixel(1, 1), checker.getPixel(0, 0));
    EXPECT_THROW(ImageFilters::pixelate(checker, 0), std::invalid_argument);
}

// The outline covers the pixels within its thickness of what shows, fades over the pixel past it, and stays under the pixels that show.
TEST_F(ImageFiltersTest, OutlinesWhatShows) {
    Image image(9, 9);
    image.setPixel(4, 4, haylen::math::Color::fromHex(0xFFFFFFFFU));
    ImageFilters::outline(image, haylen::math::Color::fromHex(0x000000FFU), 1.0F);
    EXPECT_EQ(image.getPixel(4, 4).toHex(), "#FFFFFFFF");
    EXPECT_EQ(image.getPixel(5, 4).toHex(), "#FF000000");
    EXPECT_GT(image.getPixel(5, 5).a, 0.5F);
    EXPECT_LT(image.getPixel(5, 5).a, 0.7F);
    EXPECT_EQ(image.getPixel(7, 4).a, 0.0F);
    EXPECT_THROW(ImageFilters::outline(image, haylen::math::Color::black(), ImageFilters::kMaxThickness + 1.0F), std::invalid_argument);
}

// An amount or a color that is not finite would write no number into the pixels, so the effects refuse it and leave the image as it was.
TEST_F(ImageFiltersTest, RefusesValuesThatAreNotFinite) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    Image image = makeFlat(4, 4, 0x336699FFU);
    EXPECT_THROW(ImageFilters::adjust(image, {.brightness = nan}), std::invalid_argument);
    EXPECT_THROW(ImageFilters::adjust(image, {.hue = infinity}), std::invalid_argument);
    EXPECT_THROW(ImageFilters::grayscale(image, nan), std::invalid_argument);
    EXPECT_THROW(ImageFilters::tint(image, haylen::math::Color::white(), nan), std::invalid_argument);
    EXPECT_THROW(ImageFilters::tint(image, {nan, 1.0F, 1.0F, 1.0F}, 1.0F), std::invalid_argument);
    EXPECT_THROW(ImageFilters::outline(image, {1.0F, 1.0F, 1.0F, nan}, 1.0F), std::invalid_argument);
    EXPECT_THROW(ImageFilters::sharpen(image, infinity), std::invalid_argument);
    EXPECT_THROW(ImageFilters::blur(image, nan), std::invalid_argument);
    EXPECT_EQ(image.getPixel(1, 1).toHex(), "#FF336699");
}

// An effect changes the pixels the mipmaps were made from, so the image drops them and a texture made from it makes them again.
TEST_F(ImageFiltersTest, DropsTheMipmapsOfTheImageItChanges) {
    Image image(8, 8, haylen::math::Color::white());
    image.buildMipmaps();
    ASSERT_FALSE(image.getMipmaps().empty());
    ImageFilters::invert(image);
    EXPECT_TRUE(image.getMipmaps().empty());
}

} // namespace haylen::graphics
