#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/ImagePalette.hpp"
#include "haylen/graphics/ImagePlaceholder.hpp"
#include "haylen/math/Color.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::graphics {

namespace {

// A cover of three bands, a wide deep blue, a vivid orange and a pale sand, over a transparent border that shows nothing.
Image makeCover() {
    Image image(120, 100);
    image.fill({10, 10, 100, 50}, math::Color::fromHex(0x1E3A8AFFU));
    image.fill({10, 60, 100, 25}, math::Color::fromHex(0xF97316FFU));
    image.fill({10, 85, 100, 10}, math::Color::fromHex(0xE8E2D6FFU));
    return image;
}

} // namespace

TEST(ImagePaletteTest, FindsTheColorsOfTheVisiblePixelsTheMostCommonFirst) {
    const std::vector<ImagePalette::Swatch> palette = ImagePalette::extract(makeCover(), 8);
    ASSERT_EQ(palette.size(), 3U);
    EXPECT_EQ(palette[0].color.toHex(), "#FF1E3A8A");
    EXPECT_EQ(palette[1].color.toHex(), "#FFF97316");
    EXPECT_EQ(palette[2].color.toHex(), "#FFE8E2D6");
    EXPECT_NEAR(palette[0].population, 0.59F, 0.03F);
    EXPECT_NEAR(palette[0].population + palette[1].population + palette[2].population, 1.0F, 0.0001F);

    EXPECT_EQ(ImagePalette::extract(makeCover(), 1).size(), 1U);
    EXPECT_TRUE(ImagePalette::extract(Image(16, 16), 4).empty());
    EXPECT_THROW((void)ImagePalette::extract(makeCover(), 0), std::invalid_argument);
    EXPECT_THROW((void)ImagePalette::extract(makeCover(), ImagePalette::kMaxColors + 1), std::invalid_argument);
}

TEST(ImagePaletteTest, ChoosesTheSwatchesThatInterfacesTakeTheirColorsFrom) {
    const ImagePalette::Swatches swatches = ImagePalette::choose(ImagePalette::extract(makeCover(), 8));
    EXPECT_EQ(swatches.dominant.toHex(), "#FF1E3A8A");
    ASSERT_TRUE(swatches.vibrant.has_value());
    EXPECT_EQ(swatches.vibrant->toHex(), "#FFF97316");
    ASSERT_TRUE(swatches.darkVibrant.has_value());
    EXPECT_EQ(swatches.darkVibrant->toHex(), "#FF1E3A8A");
    ASSERT_TRUE(swatches.lightMuted.has_value());
    EXPECT_EQ(swatches.lightMuted->toHex(), "#FFE8E2D6");
    EXPECT_FALSE(swatches.muted.has_value());

    const ImagePalette::Swatches none = ImagePalette::choose({});
    EXPECT_EQ(none.dominant, math::Color::transparent());
    EXPECT_FALSE(none.vibrant.has_value());
}

// A placeholder keeps the average color and the layout of the colors, so the blurred image shows the blue above and the orange below.
TEST(ImagePlaceholderTest, EncodesAShortTextThatDecodesToABlurredImage) {
    const std::string code = ImagePlaceholder::encode(makeCover(), {});
    EXPECT_EQ(code.size(), 36U);
    EXPECT_EQ(code.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"), std::string::npos);
    EXPECT_EQ(ImagePlaceholder::encode(makeCover(), {}), code);

    const Image blurred = ImagePlaceholder::decode(code, 24, 20);
    EXPECT_EQ(blurred.getWidth(), 24);
    EXPECT_EQ(blurred.getHeight(), 20);
    const math::Color top = blurred.getPixel(12, 6);
    const math::Color bottom = blurred.getPixel(12, 15);
    EXPECT_GT(top.b, top.r);
    EXPECT_GT(bottom.r, bottom.b);
    EXPECT_EQ(top.a, 1.0F);

    const std::string tiny = ImagePlaceholder::encode(makeCover(), {.columns = 1, .rows = 1});
    EXPECT_EQ(tiny.size(), 7U);
    const math::Color flat = ImagePlaceholder::decode(tiny, 4, 4).getPixel(0, 0);
    EXPECT_EQ(flat, ImagePlaceholder::decode(tiny, 4, 4).getPixel(3, 3));
}

// A placeholder may come from a server, so every malformed text and every size out of range is refused without reading past its bytes.
TEST(ImagePlaceholderTest, RefusesHostileTextsAndSizes) {
    const std::string code = ImagePlaceholder::encode(makeCover(), {});
    for (const std::string& hostile : {std::string(), std::string("AAAA"), code.substr(0, code.size() - 3), code + "AAAA", std::string(code).replace(0, 1, "*"), std::string("_") + code.substr(1), std::string(500, 'A')}) {
        EXPECT_THROW((void)ImagePlaceholder::decode(hostile, 8, 8), std::invalid_argument) << hostile;
    }
    EXPECT_THROW((void)ImagePlaceholder::decode(code, 0, 8), std::invalid_argument);
    EXPECT_THROW((void)ImagePlaceholder::decode(code, 8, ImagePlaceholder::kMaxSize + 1), std::invalid_argument);
    EXPECT_THROW((void)ImagePlaceholder::encode(Image(), {}), std::invalid_argument);
    EXPECT_THROW((void)ImagePlaceholder::encode(makeCover(), {.columns = 10}), std::invalid_argument);
}

TEST(ImagePaletteLuaTest, ReadsThePaletteSwatchesAndPlaceholdersOfAnImage) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        graphics = require('haylen.graphics')
        cover = graphics.newImage(120, 100)
        cover:fill('#FF1E3A8A', {10, 10, 100, 50})
        cover:fill('#FFF97316', {10, 60, 100, 25})
        cover:fill('#FFE8E2D6', {10, 85, 100, 10})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("local palette = cover:palette() return #palette .. ' ' .. palette[1].color:toHex() .. ' ' .. string.format('%.2f', palette[1].population)"), "3 #FF1E3A8A 0.58");
    EXPECT_EQ(fixture.lua("local swatches = cover:swatches() return swatches.dominant:toHex() .. ' ' .. swatches.vibrant:toHex() .. ' ' .. tostring(swatches.muted)"), "#FF1E3A8A #FFF97316 nil");
    EXPECT_EQ(fixture.lua("local code = cover:placeholder({columns = 3, rows = 2}) local image = graphics.decodePlaceholder(code, 30, 20) return #code .. ' ' .. image.width .. ' ' .. image.height"), "20 30 20");
    EXPECT_NE(fixture.lua("cover:placeholder({colums = 3})").find("Unknown option \"colums\"."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.decodePlaceholder('nope!', 8, 8)").find("The text is not an image placeholder."), std::string::npos);
    EXPECT_NE(fixture.lua("cover:palette(0)").find("A palette has from 1 to 64 colors, not 0."), std::string::npos);
}

} // namespace haylen::graphics
