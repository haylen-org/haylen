#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/Layout.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::text {

namespace {

// Loads the color test fonts: a subset of an emoji font whose `COLR` table has version 1, with gradients, transforms and the ligatures of emoji sequences, and a subset of a display font whose letters are the layers of version 0.
class ColorGlyphTest : public ::testing::Test {
  protected:
    static constexpr std::string_view kEmoji = "noto-color-emoji-regular.ttf";
    static constexpr std::string_view kLayers = "bungee-color-regular.ttf";

    [[nodiscard]] static std::vector<std::uint8_t> read(std::string_view name) {
        std::ifstream file(std::string(HAYLEN_TEST_FONTS) + "/" + std::string(name), std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    [[nodiscard]] std::shared_ptr<TrueTypeFont> load(std::string_view name) {
        return std::make_shared<TrueTypeFont>(fixture.engine().getGraphics(), read(name));
    }

    // Counts the opaque pixels of a raster whose color lies within a tolerance of a color.
    [[nodiscard]] static std::size_t countNear(const graphics::Image& raster, std::uint32_t rgb, int tolerance) {
        const std::span<const std::uint8_t> pixels = raster.getPixels();
        std::size_t count = 0;
        for (std::size_t index = 0; index < pixels.size(); index += 4) {
            const bool red = std::abs(pixels[index] - static_cast<int>(rgb >> 16U & 0xFFU)) <= tolerance;
            const bool green = std::abs(pixels[index + 1] - static_cast<int>(rgb >> 8U & 0xFFU)) <= tolerance;
            const bool blue = std::abs(pixels[index + 2] - static_cast<int>(rgb & 0xFFU)) <= tolerance;
            count += pixels[index + 3] == 255 && red && green && blue ? 1 : 0;
        }
        return count;
    }

    [[nodiscard]] static std::size_t countOpaque(const graphics::Image& raster) {
        const std::span<const std::uint8_t> pixels = raster.getPixels();
        std::size_t count = 0;
        for (std::size_t index = 3; index < pixels.size(); index += 4) {
            count += pixels[index] == 255 ? 1 : 0;
        }
        return count;
    }

    test::EngineFixture fixture;
};

} // namespace

// A glyph of the version 1 table paints its gradients in many shades, fills the box of its image, and leaves the corners around its round face clear.
TEST_F(ColorGlyphTest, PaintsTheGradientsOfColorGlyphs) {
    const std::shared_ptr<TrueTypeFont> emoji = load(kEmoji);
    EXPECT_TRUE(emoji->hasGlyph(U'\U0001F600'));
    EXPECT_TRUE(emoji->hasColorGlyph(U'\U0001F600'));
    EXPECT_FALSE(fixture.engine().getDefaultFont()->hasColorGlyph(U'A'));

    const std::shared_ptr<const Layout> laid = emoji->layout("\xF0\x9F\x98\x80", {.size = 48.0F});
    ASSERT_EQ(laid->glyphs.size(), 1U);
    const Layout::Glyph& glyph = laid->glyphs[0];
    ASSERT_TRUE(glyph.image.isValid());
    EXPECT_EQ(glyph.source.getSize(), glyph.image.getSize());
    EXPECT_EQ(glyph.size, glyph.image.getSize());

    const graphics::Image raster = glyph.image.rasterize(2.0F);
    const auto [width, height] = graphics::VectorImage::getRasterSize(glyph.image.getSize(), 2.0F);
    ASSERT_EQ(raster.getWidth(), width);
    ASSERT_EQ(raster.getHeight(), height);
    EXPECT_EQ(raster.getPixel(0, 0).a, 0.0F);
    EXPECT_EQ(raster.getPixel(width - 1, height - 1).a, 0.0F);

    // The face is a radial gradient of yellows, so its opaque pixels take many colors.
    const std::span<const std::uint8_t> pixels = raster.getPixels();
    std::set<std::uint32_t> yellows;
    for (std::size_t index = 0; index < pixels.size(); index += 4) {
        if (pixels[index + 3] == 255 && pixels[index] > 200 && pixels[index + 1] > 150 && pixels[index + 2] < 120) {
            yellows.insert(static_cast<std::uint32_t>(pixels[index]) << 16U | static_cast<std::uint32_t>(pixels[index + 1]) << 8U | pixels[index + 2]);
        }
    }
    EXPECT_GT(yellows.size(), 20U);
}

// The letters of a font of version 0 are layers in the colors of the first palette of its `CPAL` table.
TEST_F(ColorGlyphTest, PaintsTheLayersOfVersionZeroInTheColorsOfThePalette) {
    const std::shared_ptr<TrueTypeFont> layers = load(kLayers);
    EXPECT_TRUE(layers->hasColorGlyph(U'A'));
    const std::shared_ptr<const Layout> laid = layers->layout("A", {.size = 48.0F});
    ASSERT_EQ(laid->glyphs.size(), 1U);
    ASSERT_TRUE(laid->glyphs[0].image.isValid());

    // The pink layer is a thin line inside the red letter, which covers whole pixels once the letter is large.
    const graphics::Image raster = laid->glyphs[0].image.rasterize(8.0F);
    EXPECT_GT(countNear(raster, 0xC90900U, 4), 10000U);
    EXPECT_GT(countNear(raster, 0xFF9580U, 4), 500U);
}

// The image of a color glyph keeps the font file alive, so a raster made after its font is gone still paints.
TEST_F(ColorGlyphTest, KeepsTheFontFileAliveForItsImages) {
    graphics::VectorImage image;
    {
        const std::shared_ptr<TrueTypeFont> emoji = load(kEmoji);
        image = emoji->layout("\xF0\x9F\x9A\x80", {.size = 32.0F})->glyphs.at(0).image;
    }
    ASSERT_TRUE(image.isValid());
    EXPECT_GT(countOpaque(image.rasterize(1.0F)), 100U);
}

// A sequence joined by zero-width joiners is one character, which draws as the one glyph of its ligature when the font has it and as its emoji side by side otherwise.
TEST_F(ColorGlyphTest, DrawsEmojiSequencesAsTheGlyphOfTheirLigature) {
    const std::shared_ptr<TrueTypeFont> emoji = load(kEmoji);
    const std::shared_ptr<const Layout> technologist = emoji->layout("\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB", {.size = 32.0F});
    ASSERT_EQ(technologist->glyphs.size(), 1U);
    EXPECT_EQ(technologist->characters.size(), 1U);
    EXPECT_TRUE(technologist->glyphs[0].image.isValid());

    const std::shared_ptr<const Layout> family = emoji->layout("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7", {.size = 32.0F});
    EXPECT_EQ(family->glyphs.size(), 1U);
    EXPECT_EQ(family->characters.size(), 1U);

    // A thumb with a skin tone is a ligature too, and a thumb joined to a fire has none.
    EXPECT_EQ(emoji->layout("\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD", {.size = 32.0F})->glyphs.size(), 1U);
    const std::shared_ptr<const Layout> joined = emoji->layout("\xF0\x9F\x91\x8D\xE2\x80\x8D\xF0\x9F\x94\xA5", {.size = 32.0F});
    EXPECT_EQ(joined->glyphs.size(), 2U);
    EXPECT_EQ(joined->characters.size(), 1U);
}

// Text in a family takes its letters from the face and its emoji from the color fallback, and the default presentation of a character and the variation selectors choose between a symbol in the color of the text and its color emoji.
TEST_F(ColorGlyphTest, FallsBackToColorEmojiByThePresentationTheTextAsks) {
    const std::shared_ptr<TrueTypeFont> emoji = load(kEmoji);
    const std::shared_ptr<TrueTypeFont> symbols = load("noto-sans-symbols-2-regular.ttf");
    FontFamily symbolsFirst({.regular = fixture.engine().getDefaultFont(), .fallbacks = {symbols, emoji}});
    FontFamily emojiFirst({.regular = fixture.engine().getDefaultFont(), .fallbacks = {emoji, symbols}});

    const std::shared_ptr<const Layout> mixed = symbolsFirst.layout("Hi \xF0\x9F\x98\x80", {.size = 32.0F});
    ASSERT_EQ(mixed->glyphs.size(), 3U);
    EXPECT_FALSE(mixed->glyphs[0].image.isValid());
    EXPECT_EQ(mixed->looks[mixed->glyphs[2].look].font, emoji.get());
    EXPECT_TRUE(mixed->glyphs[2].image.isValid());

    // The sun is a symbol of the first font that has it unless U+FE0F asks for the emoji, and U+FE0E asks for the symbol even after an emoji font.
    EXPECT_FALSE(symbolsFirst.layout("\xE2\x98\x80", {.size = 32.0F})->glyphs.at(0).image.isValid());
    EXPECT_TRUE(symbolsFirst.layout("\xE2\x98\x80\xEF\xB8\x8F", {.size = 32.0F})->glyphs.at(0).image.isValid());
    EXPECT_TRUE(emojiFirst.layout("\xE2\x98\x80", {.size = 32.0F})->glyphs.at(0).image.isValid());
    EXPECT_FALSE(emojiFirst.layout("\xE2\x98\x80\xEF\xB8\x8E", {.size = 32.0F})->glyphs.at(0).image.isValid());

    // A star shows as an emoji by default, so it takes the emoji font even after a symbol font that has it, unless U+FE0E asks for its text form.
    EXPECT_TRUE(symbolsFirst.layout("\xE2\xAD\x90", {.size = 32.0F})->glyphs.at(0).image.isValid());
    EXPECT_FALSE(symbolsFirst.layout("\xE2\xAD\x90\xEF\xB8\x8E", {.size = 32.0F})->glyphs.at(0).image.isValid());
}

} // namespace haylen::text
