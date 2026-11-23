#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/RichText.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"
#include "support/FontCollection.hpp"

namespace haylen::text {

// Lays out text whose glyphs reach past the lines of their fonts: the Thai and Latin test fonts with line metrics far lower than their letters and stacked marks, next to the same fonts as they are.
class LineHeightTest : public ::testing::Test {
  protected:
    [[nodiscard]] std::shared_ptr<Font> load(std::string_view name) {
        return std::make_shared<TrueTypeFont>(fixture.engine().getGraphics(), test::FontCollection::read(name));
    }

    [[nodiscard]] std::shared_ptr<Font> lowered(std::string_view name, std::int16_t ascender, std::int16_t descender) {
        return std::make_shared<TrueTypeFont>(fixture.engine().getGraphics(), test::FontCollection::setLineMetrics(test::FontCollection::read(name), ascender, descender, 0));
    }

    [[nodiscard]] RichText make(const std::string& markup, const std::shared_ptr<Font>& font, RichTextOptions options) {
        options.family = std::make_shared<FontFamily>(FontFamily::Faces{.regular = font});
        return {markup, std::move(options), fixture.engine().getPlugin<plugins::TextPlugin>().getRegistry()};
    }

    // The top and the bottom of the ink of a laid out glyph, which its font measures from the pen at its native size.
    [[nodiscard]] static std::pair<float, float> inkOf(const Layout& layout, const Layout::Glyph& glyph) {
        const Layout::Look& look = layout.looks[glyph.look];
        const math::Rect ink = look.font->getGlyph(glyph.index).ink;
        const float factor = look.size / look.font->getNativeSize();
        return {glyph.baseline + ink.y * factor, glyph.baseline + ink.getBottom() * factor};
    }

    // Every glyph draws its ink inside the box of its line, and every line starts below the end of the line before it.
    static void expectInkInsideLines(const Layout& layout) {
        for (const Layout::Glyph& glyph : layout.glyphs) {
            for (const Layout::Line& line : layout.lines) {
                if (glyph.character < line.firstCharacter || glyph.character >= line.endCharacter) {
                    continue;
                }

                const auto [top, bottom] = inkOf(layout, glyph);
                EXPECT_GE(top, line.box.y - 0.01F) << "Glyph " << glyph.index << ".";
                EXPECT_LE(bottom, line.box.getBottom() + 0.01F) << "Glyph " << glyph.index << ".";
            }
        }

        for (std::size_t index = 1; index < layout.lines.size(); ++index) {
            EXPECT_GE(layout.lines[index].box.y, layout.lines[index - 1].box.getBottom() - 0.01F) << "Line " << index << ".";
        }
    }

    test::EngineFixture fixture;
};

// The tone mark of Thai stacks on a vowel above the letter, far above lines of a third of the em square, so that line grows with the marks while a line of letters alone grows only to their tops, and both hold all of their ink.
TEST_F(LineHeightTest, GrowsANaturalLineWithTheInkOfStackedMarks) {
    const std::shared_ptr<Font> thai = lowered("noto-sans-thai-regular.ttf", 300, -100);
    const std::shared_ptr<const Layout> laid = thai->layout("ที่ดี\nกา", {.size = 40.0F, .language = "th"});
    ASSERT_EQ(laid->lines.size(), 2U);
    EXPECT_GT(laid->lines[0].box.height, thai->getLineHeight(40.0F) * 2.0F);
    EXPECT_GT(laid->lines[1].box.height, thai->getLineHeight(40.0F));
    EXPECT_LT(laid->lines[1].box.height, laid->lines[0].box.height);
    expectInkInsideLines(*laid);

    // The same text in the font as it is stays within its metrics, so every line keeps the height of the font.
    const std::shared_ptr<Font> regular = load("noto-sans-thai-regular.ttf");
    const std::shared_ptr<const Layout> kept = regular->layout("ที่ดี\nกา", {.size = 40.0F, .language = "th"});
    for (const Layout::Line& line : kept->lines) {
        EXPECT_FLOAT_EQ(line.box.height, regular->getLineHeight(40.0F));
        EXPECT_FLOAT_EQ(line.baseline - line.box.y, regular->getAscent(40.0F));
    }
}

// Latin letters, accents, descenders and brackets stay inside the metrics of their fonts, so their lines lay out exactly by those metrics.
TEST_F(LineHeightTest, KeepsLatinLinesAtTheMetricsOfTheirFonts) {
    for (const std::shared_ptr<Font>& font : {fixture.engine().getDefaultFont(), load("crimson-text-regular.ttf"), load("fira-sans-regular.otf")}) {
        for (const LineHeight lineHeight : {LineHeight::Natural, LineHeight::Uniform}) {
            const std::shared_ptr<const Layout> laid = font->layout("Hg jáé (|)\nQuy", {.size = 30.0F, .lineHeight = lineHeight});
            ASSERT_EQ(laid->lines.size(), 2U);

            for (const Layout::Line& line : laid->lines) {
                EXPECT_FLOAT_EQ(line.box.height, font->getLineHeight(30.0F));
                EXPECT_FLOAT_EQ(line.baseline - line.box.y, font->getAscent(30.0F));
            }

            EXPECT_FLOAT_EQ(laid->size.y, font->getLineHeight(30.0F) * 2.2F);
        }
    }
}

// Each line of a paragraph of mixed sizes takes the ink of its own glyphs at their sizes, and a uniform line height gives both lines the height of the taller one, so the baselines follow one step.
TEST_F(LineHeightTest, SizesMixedLinesByTheirOwnInkOrByTheTallest) {
    const std::shared_ptr<Font> crimson = lowered("crimson-text-regular.ttf", 300, -100);
    const std::string markup = "small [size=60]Hg[/size] text\nsmall";
    RichText natural = make(markup, crimson, {.size = 20.0F});
    const Layout& mixed = natural.getLayout();
    ASSERT_EQ(mixed.lines.size(), 2U);
    EXPECT_GT(mixed.lines[0].box.height, mixed.lines[1].box.height);
    EXPECT_GT(mixed.lines[1].box.height, crimson->getLineHeight(20.0F));
    expectInkInsideLines(mixed);

    RichText uniform = make(markup, crimson, {.size = 20.0F, .lineHeight = LineHeight::Uniform});
    const Layout& even = uniform.getLayout();
    ASSERT_EQ(even.lines.size(), 2U);
    EXPECT_FLOAT_EQ(even.lines[0].box.height, mixed.lines[0].box.height);
    EXPECT_FLOAT_EQ(even.lines[1].box.height, mixed.lines[0].box.height);
    EXPECT_FLOAT_EQ(even.lines[1].baseline - even.lines[0].baseline, mixed.lines[0].box.height * 1.2F);
    expectInkInsideLines(even);
}

// A uniform line height spans the whole text: every paragraph, the empty line between them and the lines of a table take the tallest ascent and descent of all of them.
TEST_F(LineHeightTest, GivesEveryLineOfAUniformTextTheTallestHeight) {
    const std::shared_ptr<Font> thai = lowered("noto-sans-thai-regular.ttf", 300, -100);
    const std::shared_ptr<const Layout> natural = thai->layout("กา\nที่ดี\n\nกา", {.size = 40.0F, .language = "th"});
    const std::shared_ptr<const Layout> uniform = thai->layout("กา\nที่ดี\n\nกา", {.size = 40.0F, .lineHeight = LineHeight::Uniform, .language = "th"});
    ASSERT_EQ(uniform->lines.size(), 4U);
    const float tallest = natural->lines[1].box.height;
    for (std::size_t index = 0; index < uniform->lines.size(); ++index) {
        EXPECT_FLOAT_EQ(uniform->lines[index].box.height, tallest) << "Line " << index << ".";
        EXPECT_FLOAT_EQ(uniform->lines[index].baseline - uniform->lines[index].box.y, natural->lines[1].baseline - natural->lines[1].box.y) << "Line " << index << ".";
    }

    EXPECT_FLOAT_EQ(uniform->size.y, tallest * (3.0F * 1.2F + 1.0F));
    expectInkInsideLines(*uniform);

    RichText table = make("[table=2][cell]กา[/cell][cell]ที่ดี[/cell][/table]\nกา", thai, {.size = 40.0F, .language = "th", .lineHeight = LineHeight::Uniform});
    const Layout& cells = table.getLayout();
    ASSERT_EQ(cells.lines.size(), 3U);
    for (const Layout::Line& line : cells.lines) {
        EXPECT_FLOAT_EQ(line.box.height, tallest);
    }
}

// The line height is part of the key of a cached layout, and a layout drawn again unchanged in either mode allocates nothing.
TEST_F(LineHeightTest, CachesEachLineHeightApartWithoutAllocatingAgain) {
    const std::shared_ptr<Font> thai = lowered("noto-sans-thai-regular.ttf", 300, -100);
    const Style natural{.size = 40.0F, .language = "th"};
    const Style uniform{.size = 40.0F, .lineHeight = LineHeight::Uniform, .language = "th"};
    const std::shared_ptr<const Layout> first = thai->layout("กา\nที่ดี", natural);
    const std::shared_ptr<const Layout> even = thai->layout("กา\nที่ดี", uniform);
    EXPECT_NE(first, even);
    EXPECT_NE(first->size.y, even->size.y);

    RichText rich = make("กา\nที่ดี", thai, {.size = 40.0F, .language = "th", .lineHeight = LineHeight::Uniform});
    static_cast<void>(rich.getLayout());
    const test::AllocationTracker allocations;
    for (int frame = 0; frame < 3; ++frame) {
        EXPECT_EQ(thai->layout("กา\nที่ดี", natural), first);
        EXPECT_EQ(thai->layout("กา\nที่ดี", uniform), even);
        rich.update(1.0F / 60.0F);
        static_cast<void>(rich.getLayout());
    }

    EXPECT_EQ(allocations.getTotal(), 0U);
}

} // namespace haylen::text
