#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/ImageFilters.hpp"
#include "haylen/math/Color.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::graphics {

class ImageFiltersTest : public ::testing::Test {
  protected:
    [[nodiscard]] static haylen::graphics::Image makeFlat(int width, int height, std::uint32_t color) {
        return haylen::graphics::Image(width, height, haylen::math::Color::fromHex(color));
    }
};

// A blur spreads a block over its surroundings, and since it mixes premultiplied colors the transparent pixels around a red block never turn its edge dark.
TEST_F(ImageFiltersTest, BlursWithoutDarkeningTransparentEdges) {
    haylen::graphics::Image image(21, 21);
    image.fill({8, 8, 5, 5}, haylen::math::Color::fromHex(0xFF0000FFU));
    haylen::graphics::ImageFilters::blur(image, 2.0F);
    const haylen::math::Color center = image.getPixel(10, 10);
    const haylen::math::Color edge = image.getPixel(6, 10);
    EXPECT_LT(center.a, 1.0F);
    EXPECT_GT(center.a, 0.5F);
    EXPECT_GT(edge.a, 0.0F);
    EXPECT_LT(edge.a, center.a);
    EXPECT_EQ(edge.r, 1.0F);
    EXPECT_EQ(edge.g, 0.0F);
    EXPECT_EQ(image.getPixel(0, 0).a, 0.0F);

    haylen::graphics::Image flat = makeFlat(8, 8, 0x336699FFU);
    haylen::graphics::ImageFilters::blur(flat, 3.0F);
    EXPECT_EQ(flat.getPixel(4, 4).toHex(), "#FF336699");
    EXPECT_THROW(haylen::graphics::ImageFilters::blur(flat, -1.0F), std::invalid_argument);
    EXPECT_THROW(haylen::graphics::ImageFilters::blur(flat, haylen::graphics::ImageFilters::kMaxRadius + 1.0F), std::invalid_argument);
}

TEST_F(ImageFiltersTest, SharpensTheEdgesBetweenColors) {
    haylen::graphics::Image image = makeFlat(10, 4, 0x808080FFU);
    image.fill({5, 0, 5, 4}, haylen::math::Color::fromHex(0x404040FFU));
    haylen::graphics::ImageFilters::sharpen(image, 1.0F);
    EXPECT_GT(image.getPixel(4, 2).r, 0.502F);
    EXPECT_LT(image.getPixel(5, 2).r, 0.25F);
    EXPECT_NEAR(image.getPixel(0, 2).r, 0.502F, 0.004F);
    EXPECT_THROW(haylen::graphics::ImageFilters::sharpen(image, -1.0F), std::invalid_argument);
}

TEST_F(ImageFiltersTest, AdjustsBrightnessContrastSaturationAndHue) {
    haylen::graphics::Image gray = makeFlat(1, 1, 0x808080FFU);
    haylen::graphics::ImageFilters::adjust(gray, {.brightness = 0.1F});
    EXPECT_NEAR(gray.getPixel(0, 0).r, 0.6F, 0.004F);

    haylen::graphics::Image light = makeFlat(1, 1, 0xBFBFBFFFU);
    haylen::graphics::ImageFilters::adjust(light, {.contrast = 2.0F});
    EXPECT_NEAR(light.getPixel(0, 0).r, 1.0F, 0.004F);

    haylen::graphics::Image red = makeFlat(1, 1, 0xFF0000FFU);
    haylen::graphics::ImageFilters::adjust(red, {.hue = 1.0F / 3.0F});
    EXPECT_EQ(red.getPixel(0, 0).toHex(), "#FF00FF00");

    haylen::graphics::Image vivid = makeFlat(1, 1, 0xFF0000FFU);
    haylen::graphics::ImageFilters::grayscale(vivid, 1.0F);
    const haylen::math::Color luma = vivid.getPixel(0, 0);
    EXPECT_NEAR(luma.r, 0.2126F, 0.004F);
    EXPECT_EQ(luma.r, luma.g);
    EXPECT_EQ(luma.g, luma.b);
}

TEST_F(ImageFiltersTest, InvertsTintsAndPixelates) {
    haylen::graphics::Image image = makeFlat(1, 1, 0x0A141E80U);
    haylen::graphics::ImageFilters::invert(image);
    EXPECT_EQ(image.getPixel(0, 0).toHex(), "#80F5EBE1");

    haylen::graphics::Image red = makeFlat(1, 1, 0xFF0000FFU);
    haylen::graphics::ImageFilters::tint(red, haylen::math::Color::fromHex(0x0000FFFFU), 0.5F);
    EXPECT_EQ(red.getPixel(0, 0).toHex(), "#FF800080");

    haylen::graphics::Image checker(2, 2);
    checker.setPixel(0, 0, haylen::math::Color::fromHex(0xFF0000FFU));
    checker.setPixel(1, 0, haylen::math::Color::fromHex(0x00FF00FFU));
    checker.setPixel(0, 1, haylen::math::Color::fromHex(0x0000FFFFU));
    checker.setPixel(1, 1, haylen::math::Color::fromHex(0xFFFFFFFFU));
    haylen::graphics::ImageFilters::pixelate(checker, 2);
    EXPECT_EQ(checker.getPixel(0, 0).toHex(), "#FF808080");
    EXPECT_EQ(checker.getPixel(1, 1), checker.getPixel(0, 0));
    EXPECT_THROW(haylen::graphics::ImageFilters::pixelate(checker, 0), std::invalid_argument);
}

// The outline covers the pixels within its thickness of what shows, fades over the pixel past it, and stays under the pixels that show.
TEST_F(ImageFiltersTest, OutlinesWhatShows) {
    haylen::graphics::Image image(9, 9);
    image.setPixel(4, 4, haylen::math::Color::fromHex(0xFFFFFFFFU));
    haylen::graphics::ImageFilters::outline(image, haylen::math::Color::fromHex(0x000000FFU), 1.0F);
    EXPECT_EQ(image.getPixel(4, 4).toHex(), "#FFFFFFFF");
    EXPECT_EQ(image.getPixel(5, 4).toHex(), "#FF000000");
    EXPECT_GT(image.getPixel(5, 5).a, 0.5F);
    EXPECT_LT(image.getPixel(5, 5).a, 0.7F);
    EXPECT_EQ(image.getPixel(7, 4).a, 0.0F);
    EXPECT_THROW(haylen::graphics::ImageFilters::outline(image, haylen::math::Color::black(), haylen::graphics::ImageFilters::kMaxThickness + 1.0F), std::invalid_argument);
}

// An amount or a color that is not finite would write no number into the pixels, so the effects refuse it and leave the image as it was.
TEST_F(ImageFiltersTest, RefusesValuesThatAreNotFinite) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    haylen::graphics::Image image = makeFlat(4, 4, 0x336699FFU);
    EXPECT_THROW(haylen::graphics::ImageFilters::adjust(image, {.brightness = nan}), std::invalid_argument);
    EXPECT_THROW(haylen::graphics::ImageFilters::adjust(image, {.hue = infinity}), std::invalid_argument);
    EXPECT_THROW(haylen::graphics::ImageFilters::grayscale(image, nan), std::invalid_argument);
    EXPECT_THROW(haylen::graphics::ImageFilters::tint(image, haylen::math::Color::white(), nan), std::invalid_argument);
    EXPECT_THROW(haylen::graphics::ImageFilters::tint(image, {nan, 1.0F, 1.0F, 1.0F}, 1.0F), std::invalid_argument);
    EXPECT_THROW(haylen::graphics::ImageFilters::outline(image, {1.0F, 1.0F, 1.0F, nan}, 1.0F), std::invalid_argument);
    EXPECT_THROW(haylen::graphics::ImageFilters::sharpen(image, infinity), std::invalid_argument);
    EXPECT_THROW(haylen::graphics::ImageFilters::blur(image, nan), std::invalid_argument);
    EXPECT_EQ(image.getPixel(1, 1).toHex(), "#FF336699");
}

// An effect changes the pixels the mipmaps were made from, so the image drops them and a texture made from it makes them again.
TEST_F(ImageFiltersTest, DropsTheMipmapsOfTheImageItChanges) {
    haylen::graphics::Image image(8, 8, haylen::math::Color::white());
    image.buildMipmaps();
    ASSERT_FALSE(image.getMipmaps().empty());
    haylen::graphics::ImageFilters::invert(image);
    EXPECT_TRUE(image.getMipmaps().empty());
}

TEST(ImageFiltersLuaTest, AppliesEffectsInPlaceAndOnTheTaskPool) {
    test::LuaFixture fixture;
    fixture.runLua("graphics = require('haylen.graphics') async = require('async') image = graphics.newImage(4, 4, '#FF0A141E')");
    EXPECT_EQ(fixture.lua("image:invert() return image:pixel(0, 0):toHex()"), "#FFF5EBE1");
    EXPECT_EQ(fixture.lua("image:adjust({saturation = 0}) image:blur(1) image:grayscale() image:tint('#FF000000', 0) image:pixelate(2) image:sharpen() image:outline('#FF000000', 1) return image.width"), "4");
    // clang-format off
    fixture.runLua(R"(
        async.spawn(function()
            local result = image:applyAsync({{effect = 'invert'}, {effect = 'pixelate', size = 2}}):await()
            done = result:pixel(0, 0):toHex() .. ' ' .. image:pixel(0, 0):toHex()
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(done)") != "nil"; }));
    EXPECT_EQ(fixture.lua("local inverted, original = done:match('(%S+) (%S+)') return tostring(inverted ~= original)"), "true");
    EXPECT_NE(fixture.lua("image:applyAsync({{effect = 'melt'}})").find("The image effect \"melt\" is unknown."), std::string::npos);
    EXPECT_NE(fixture.lua("image:applyAsync({{effect = 'blur', radius = 2, sigma = 1}})").find("Unknown option \"sigma\"."), std::string::npos);
    EXPECT_NE(fixture.lua("image:applyAsync({{effect = 'pixelate', size = 1e20}})").find("size"), std::string::npos);
    EXPECT_NE(fixture.lua("image:adjust({gamma = 2})").find("Unknown option \"gamma\"."), std::string::npos);
    EXPECT_NE(fixture.lua("image:blur(-1)").find("A blur has a radius from 0 to 64 pixels."), std::string::npos);
    EXPECT_NE(fixture.lua("image:adjust({contrast = 0 / 0})").find("An adjustment of the colors has a finite brightness, contrast, saturation and hue."), std::string::npos);
    EXPECT_NE(fixture.lua("image:grayscale(math.huge)").find("A grayscale has a finite amount."), std::string::npos);
    EXPECT_NE(fixture.lua("image:tint('#FF000000', 0 / 0)").find("A tint has a finite amount and a color of finite channels."), std::string::npos);
}

} // namespace haylen::lua::graphics
