#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include "core/EmbeddedFiles.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::text {

TEST(FontTest, LaysOutWrapsAndAlignsText) {
    test::EngineFixture fixture;
    text::Font& font = *fixture.engine().getDefaultFont();

    const text::TextLayout single = font.layout("Hello", {.size = 32.0F});
    EXPECT_EQ(single.lineCount, 1U);
    EXPECT_EQ(single.quads.size(), 5U);
    EXPECT_GT(single.size.x, 40.0F);
    EXPECT_NEAR(single.size.y, font.getLineHeight(32.0F), 0.01F);

    const text::TextLayout wrapped = font.layout("one two three four five", {.size = 32.0F, .maxWidth = 120.0F});
    EXPECT_GT(wrapped.lineCount, 2U);
    EXPECT_FLOAT_EQ(wrapped.size.x, 120.0F);

    const text::TextLayout lines = font.layout("a\n\nb", {.size = 20.0F});
    EXPECT_EQ(lines.lineCount, 3U);

    const text::TextLayout centered = font.layout("i\nwide text", {.size = 20.0F, .align = text::TextAlign::Center, .anchor = {0.5F, 0.5F}});
    const text::TextLayout right = font.layout("i\nwide text", {.size = 20.0F, .align = text::TextAlign::Right});
    EXPECT_LT(centered.quads.front().position.x, 0.0F);
    EXPECT_GT(right.quads.front().position.x, centered.quads.front().position.x);
    EXPECT_EQ(font.measure("Hello", {.size = 32.0F}), single.size);
    EXPECT_GT(font.getAscent(32.0F), 0.0F);
    EXPECT_GT(font.toDistance(2.0F, 32.0F), 0.0F);
    EXPECT_FLOAT_EQ(font.toDistance(4.0F, 32.0F), font.toDistance(2.0F, 16.0F));

    const text::TextLayout unicode = font.layout("Olá 世界", {.size = 24.0F});
    EXPECT_GE(unicode.quads.size(), 3U);
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

    const text::TextLayout layout = font.layout("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", {.size = 48.0F});
    font.sync();
    EXPECT_EQ(layout.quads.size(), 62U);
    EXPECT_GT(font.getPage(0).getWidth(), 64);
    EXPECT_GT(font.getPage(0).getHeight(), 64);

    for (const text::GlyphQuad& quad : layout.quads) {
        EXPECT_LE(quad.source.x + quad.source.width, static_cast<float>(font.getPage(0).getWidth()));
        EXPECT_LE(quad.source.y + quad.source.height, static_cast<float>(font.getPage(0).getHeight()));
    }
}

} // namespace haylen::text
