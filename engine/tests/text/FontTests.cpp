#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <stb_truetype.h>

#include "core/EmbeddedFiles.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "support/EngineFixture.hpp"
#include "text/DistanceField.hpp"

namespace haylen::text {

namespace {

// Reads the test fonts next to the built-in font, and opens them with stb_truetype to compare against.
class FontFileTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::vector<std::uint8_t> read(std::string_view name) {
        if (name == "default") {
            const std::span<const std::uint8_t> data = core::EmbeddedFiles::getDefaultFont();
            return {data.begin(), data.end()};
        }
        std::ifstream file(std::string(HAYLEN_TEST_FONTS) + "/" + std::string(name), std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    [[nodiscard]] static stbtt_fontinfo open(const std::vector<std::uint8_t>& data) {
        stbtt_fontinfo info{};
        EXPECT_NE(stbtt_InitFont(&info, data.data(), stbtt_GetFontOffsetForIndex(data.data(), 0)), 0);
        return info;
    }
};

} // namespace

TEST(FontTest, LaysOutWrapsAndAlignsText) {
    test::EngineFixture fixture;
    text::Font& font = *fixture.engine().getDefaultFont();

    const std::shared_ptr<const text::TextLayout> single = font.layout("Hello", {.size = 32.0F});
    EXPECT_EQ(single->lineCount, 1U);
    EXPECT_EQ(single->glyphs.size(), 5U);
    EXPECT_EQ(single->characters.size(), 5U);
    EXPECT_GT(single->size.x, 40.0F);
    EXPECT_NEAR(single->size.y, font.getLineHeight(32.0F), 0.01F);

    const std::shared_ptr<const text::TextLayout> wrapped = font.layout("one two three four five", {.size = 32.0F, .maxWidth = 120.0F});
    EXPECT_GT(wrapped->lineCount, 2U);
    EXPECT_FLOAT_EQ(wrapped->size.x, 120.0F);

    const std::shared_ptr<const text::TextLayout> lines = font.layout("a\n\nb", {.size = 20.0F});
    EXPECT_EQ(lines->lineCount, 3U);
    EXPECT_EQ(lines->characters[1].begin, 3U);

    const std::shared_ptr<const text::TextLayout> centered = font.layout("i\nwide text", {.size = 20.0F, .align = text::TextAlign::Center});
    const std::shared_ptr<const text::TextLayout> right = font.layout("i\nwide text", {.size = 20.0F, .align = text::TextAlign::Right});
    EXPECT_GT(centered->glyphs.front().position.x, 0.0F);
    EXPECT_GT(right->glyphs.front().position.x, centered->glyphs.front().position.x);
    EXPECT_EQ(font.measure("Hello", {.size = 32.0F}), single->size);
    EXPECT_GT(font.getAscent(32.0F), 0.0F);
    EXPECT_GT(font.toDistance(2.0F, 32.0F), 0.0F);
    EXPECT_FLOAT_EQ(font.toDistance(4.0F, 32.0F), font.toDistance(2.0F, 16.0F));

    const std::shared_ptr<const text::TextLayout> unicode = font.layout("Olá 世界", {.size = 24.0F});
    EXPECT_GE(unicode->glyphs.size(), 3U);
    font.sync();
    EXPECT_TRUE(font.getPage(0).isValid());

    EXPECT_THROW(text::TrueTypeFont(fixture.engine().getGraphics(), test::bytes("not a font")), std::runtime_error);
    EXPECT_THROW(text::TrueTypeFont(fixture.engine().getGraphics(), {}), std::runtime_error);
    EXPECT_THROW(text::TrueTypeFont(fixture.engine().getGraphics(), {0, 1, 0}), std::runtime_error);
}

TEST(FontTest, GrowsTheAtlasWhenGlyphsDoNotFit) {
    test::EngineFixture fixture;
    const std::span<const std::uint8_t> data = core::EmbeddedFiles::getDefaultFont();
    text::TrueTypeFont font(fixture.engine().getGraphics(), std::vector<std::uint8_t>(data.begin(), data.end()), {.bakeSize = 48.0F, .spread = 6, .atlasSize = 64});

    const std::shared_ptr<const text::TextLayout> layout = font.layout("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", {.size = 48.0F});
    font.sync();
    EXPECT_EQ(layout->glyphs.size(), 62U);
    EXPECT_GT(font.getPage(0).getWidth(), 64);
    EXPECT_GT(font.getPage(0).getHeight(), 64);

    for (const text::TextLayout::Glyph& glyph : layout->glyphs) {
        EXPECT_LE(glyph.source.x + glyph.source.width, static_cast<float>(font.getPage(0).getWidth()));
        EXPECT_LE(glyph.source.y + glyph.source.height, static_cast<float>(font.getPage(0).getHeight()));
    }
}

// The inside of a distance field covers the pixels the glyph covers, for the quadratic curves of TrueType outlines and the cubic curves of CFF outlines alike.
TEST_F(FontFileTest, BuildsDistanceFieldsFromWholeOutlines) {
    for (const std::string_view name : {"default", "fira_sans_regular.otf", "crimson_text_regular.ttf"}) {
        const std::vector<std::uint8_t> data = read(name);
        const stbtt_fontinfo info = open(data);
        const float scale = stbtt_ScaleForMappingEmToPixels(&info, 64.0F);
        for (const char32_t character : std::u32string_view(U"Sphinx&@8Og")) {
            const int glyph = stbtt_FindGlyphIndex(&info, static_cast<int>(character));
            const std::optional<DistanceField> field = DistanceField::build(info, glyph, scale, 8);
            ASSERT_TRUE(field.has_value()) << name;

            // Pixels the edge crosses may go either way, so only clearly covered and clearly empty pixels count.
            int width = 0;
            int height = 0;
            int x = 0;
            int y = 0;
            unsigned char* coverage = stbtt_GetGlyphBitmap(&info, scale, scale, glyph, &width, &height, &x, &y);
            int agreeing = 0;
            int counted = 0;
            for (int row = 0; row < height; ++row) {
                for (int column = 0; column < width; ++column) {
                    const int covered = coverage[row * width + column];
                    if (covered > 64 && covered < 192) {
                        continue;
                    }
                    const std::uint8_t distance = field->pixels[static_cast<std::size_t>((row + y - field->offsetY) * field->width + column + x - field->offsetX)];
                    agreeing += (distance >= 128) == (covered >= 192) ? 1 : 0;
                    ++counted;
                }
            }
            stbtt_FreeBitmap(coverage, nullptr);
            EXPECT_GE(agreeing, counted * 995 / 1000) << name << " " << static_cast<int>(character);
        }
        EXPECT_FALSE(DistanceField::build(info, stbtt_FindGlyphIndex(&info, ' '), scale, 8).has_value());
    }
}

// Every face sizes by its em square, so a fallback font draws at the size of the face around it, and the line metrics are those of the font at that size.
TEST_F(FontFileTest, SizesEveryFaceByItsEm) {
    test::EngineFixture fixture;
    for (const std::string_view name : {"default", "noto_sans_symbols_2_regular.ttf", "fira_sans_regular.otf"}) {
        const std::vector<std::uint8_t> data = read(name);
        const stbtt_fontinfo info = open(data);
        int ascent = 0;
        int descent = 0;
        int gap = 0;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
        int advance = 0;
        int bearing = 0;
        const int glyph = stbtt_FindGlyphIndex(&info, name == "noto_sans_symbols_2_regular.ttf" ? 0x2600 : 'M');
        stbtt_GetGlyphHMetrics(&info, glyph, &advance, &bearing);
        const float em = stbtt_ScaleForMappingEmToPixels(&info, 100.0F);

        TrueTypeFont font(fixture.engine().getGraphics(), data);
        EXPECT_NEAR(font.getAscent(100.0F), static_cast<float>(ascent) * em, 0.01F) << name;
        EXPECT_NEAR(font.getLineHeight(100.0F), static_cast<float>(ascent - descent + gap) * em, 0.01F) << name;
        EXPECT_NEAR(font.measure(name == "noto_sans_symbols_2_regular.ttf" ? "\xE2\x98\x80" : "M", {.size = 100.0F}).x, static_cast<float>(advance) * em, 0.01F) << name;
    }
}

} // namespace haylen::text
