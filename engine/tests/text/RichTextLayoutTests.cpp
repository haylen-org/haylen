#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/EmbeddedFiles.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/text/BitmapFont.hpp"
#include "haylen/text/RichText.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::text {

namespace {

// A second copy of the default font, which stands in for a real bold or italic face.
std::shared_ptr<Font> copyOfDefaultFont(core::Engine& engine) {
    const std::span<const std::uint8_t> data = core::EmbeddedFiles::getDefaultFont();
    return std::make_shared<TrueTypeFont>(engine.getGraphics(), std::vector<std::uint8_t>(data.begin(), data.end()));
}

// A grid font of two 8 by 8 cells, the second a private use character no TrueType font draws.
std::shared_ptr<Font> gridFont(core::Engine& engine) {
    const graphics::Texture texture = engine.getGraphics().createTexture(graphics::Image(16, 8, math::Color::white()));
    return std::make_shared<BitmapFont>(BitmapFont::describeGrid({.characters = "A\xEE\x80\x80", .cellWidth = 8.0F, .cellHeight = 8.0F}, texture.getSize()), std::vector<graphics::Texture>{texture});
}

class RichTextLayoutTest : public ::testing::Test {
  protected:
    [[nodiscard]] std::shared_ptr<RichTextRegistry> registry() {
        return fixture.engine().getPlugin<plugins::TextPlugin>().getRegistry();
    }

    [[nodiscard]] RichText make(const std::string& markup, RichTextOptions options = {}) {
        if (!options.family) {
            options.family = registry()->getDefaultFamily();
        }
        return {markup, std::move(options), registry()};
    }

    [[nodiscard]] Font& regular() {
        return *registry()->getDefaultFamily()->getFaces().regular;
    }

    test::EngineFixture fixture;
};

const RichTextLayout::Glyph& glyphOf(const RichTextLayout& layout, char32_t codePoint, std::size_t occurrence = 0) {
    for (const RichTextLayout::Glyph& glyph : layout.glyphs) {
        if (glyph.codePoint == codePoint && occurrence-- == 0) {
            return glyph;
        }
    }
    throw std::out_of_range("The layout has no such glyph.");
}

} // namespace

TEST_F(RichTextLayoutTest, WrapsMixedSizesOnOneBaselineAndAligns) {
    RichText mixed = make("a [size=64]B[/size] c", {.size = 20.0F});
    const RichTextLayout& line = mixed.getLayout();
    EXPECT_EQ(line.lineCount, 1U);
    EXPECT_NEAR(line.size.y, regular().getLineHeight(64.0F), 0.01F);
    EXPECT_FLOAT_EQ(glyphOf(line, U'a').baseline, glyphOf(line, U'B').baseline);
    EXPECT_FLOAT_EQ(glyphOf(line, U'c').baseline, glyphOf(line, U'B').baseline);
    EXPECT_NEAR(glyphOf(line, U'B').baseline, regular().getAscent(64.0F), 0.01F);

    RichText wrapped = make("one two three four five six seven eight nine ten", {.size = 24.0F, .maxWidth = 150.0F});
    const RichTextLayout& lines = wrapped.getLayout();
    EXPECT_GT(lines.lineCount, 2U);
    EXPECT_FLOAT_EQ(lines.size.x, 150.0F);
    for (const RichTextLayout::Glyph& glyph : lines.glyphs) {
        EXPECT_LE(glyph.position.x + glyph.size.x, 155.0F);
    }

    RichText centered = make("[center]mid[/center]\n[right]end[/right]", {.size = 24.0F, .maxWidth = 400.0F});
    const RichTextLayout& aligned = centered.getLayout();
    const float left = glyphOf(aligned, U'm').position.x;
    const float right = glyphOf(aligned, U'd').position.x + glyphOf(aligned, U'd').size.x;
    EXPECT_NEAR(left + (right - left) * 0.5F, 200.0F, 4.0F);
    EXPECT_NEAR(glyphOf(aligned, U'd', 1).position.x + glyphOf(aligned, U'd', 1).size.x, 400.0F, 6.0F);

    // Filled lines reach the right edge, and the last line of the paragraph stays at the left.
    RichText filled = make("[fill]aaa bb cccc dd eeee ff gggggg h[/fill]", {.size = 24.0F, .maxWidth = 200.0F});
    const RichTextLayout& stretched = filled.getLayout();
    ASSERT_GT(stretched.lineCount, 1U);
    float firstLineRight = 0.0F;
    for (const RichTextLayout::Glyph& glyph : stretched.glyphs) {
        if (glyph.baseline == stretched.glyphs.front().baseline) {
            firstLineRight = std::max(firstLineRight, glyph.position.x + glyph.size.x);
        }
    }
    EXPECT_NEAR(firstLineRight, 200.0F, 6.0F);
    EXPECT_LT(glyphOf(stretched, U'h').position.x, 150.0F);

    // A word wider than the line breaks between its characters.
    RichText longWord = make("abcdefghijklmnopqrstuvwxyz", {.size = 24.0F, .maxWidth = 100.0F});
    EXPECT_GT(longWord.getLayout().lineCount, 2U);
}

TEST_F(RichTextLayoutTest, LaysOutListsRulesTablesAndDropCaps) {
    RichText list = make("[ul]one\ntwo[/ul]\n[ol type=i]x[/ol]", {.size = 20.0F});
    const RichTextLayout& items = list.getLayout();
    const RichTextLayout::Glyph& bullet = glyphOf(items, U'•');
    const RichTextLayout::Glyph& first = glyphOf(items, U'o');
    EXPECT_LT(bullet.position.x + bullet.size.x, first.position.x);
    EXPECT_FLOAT_EQ(bullet.baseline, first.baseline);
    EXPECT_NEAR(first.position.x, 30.0F, 3.0F);
    EXPECT_EQ(bullet.character, first.character);
    EXPECT_GT(glyphOf(items, U'i').position.x, glyphOf(items, U'x').position.x - 20.0F);

    RichText rule = make("above\n[hr width=50% height=3 color=red]\nbelow", {.size = 20.0F, .maxWidth = 200.0F});
    const RichTextLayout& ruled = rule.getLayout();
    const auto line = std::find_if(ruled.boxes.begin(), ruled.boxes.end(), [](const RichTextLayout::Box& box) { return box.kind == RichTextLayout::Box::Kind::Rule; });
    ASSERT_NE(line, ruled.boxes.end());
    EXPECT_FLOAT_EQ(line->rect.width, 100.0F);
    EXPECT_FLOAT_EQ(line->rect.x, 50.0F);
    EXPECT_FLOAT_EQ(line->rect.height, 3.0F);
    EXPECT_GT(glyphOf(ruled, U'b', 1).position.y, line->rect.getBottom());

    RichText table = make("[table=2][cell bg=#101010 border=white]a[/cell][cell]bbbbbb[/cell][cell]c[/cell][/table]", {.size = 20.0F});
    const RichTextLayout& cells = table.getLayout();
    EXPECT_GT(glyphOf(cells, U'b').position.x, glyphOf(cells, U'a').position.x + glyphOf(cells, U'a').size.x);
    EXPECT_GT(glyphOf(cells, U'c').baseline, glyphOf(cells, U'a').baseline);
    EXPECT_NEAR(glyphOf(cells, U'c').position.x, glyphOf(cells, U'a').position.x, 2.0F);
    EXPECT_EQ(std::count_if(cells.boxes.begin(), cells.boxes.end(), [](const RichTextLayout::Box& box) { return box.kind == RichTextLayout::Box::Kind::CellBorder; }), 4);
    EXPECT_EQ(table.getLayout(40.0F).size.x, 40.0F);

    RichText dropped = make("[dropcap]W[/dropcap]ords wrap beside the tall letter and then return under it once it ends, which takes a few more lines of words to show", {.size = 16.0F, .maxWidth = 220.0F});
    const RichTextLayout& capped = dropped.getLayout();
    const RichTextLayout::Glyph& capital = glyphOf(capped, U'W');
    EXPECT_NEAR(capital.size.y, glyphOf(capped, U'o').size.y * 3.0F, capital.size.y * 0.5F);
    EXPECT_GT(glyphOf(capped, U'o').position.x, capital.position.x + capital.size.x * 0.6F);
    EXPECT_EQ(capital.character, 0U);
    float belowLeft = capped.size.x;
    for (const RichTextLayout::Glyph& glyph : capped.glyphs) {
        if (glyph.baseline > capital.position.y + capital.size.y + 16.0F) {
            belowLeft = std::min(belowLeft, glyph.position.x);
        }
    }
    EXPECT_LT(belowLeft, capital.position.x + capital.size.x);
}

TEST_F(RichTextLayoutTest, PlacesImagesAndIconsAgainstTheText) {
    const graphics::Texture wide = fixture.engine().getGraphics().createTexture(graphics::Image(20, 10, math::Color::white()));
    bool loaded = false;
    RichTextOptions options{.size = 20.0F, .images = [&](std::string_view) { return loaded ? wide : graphics::Texture{}; }};
    RichText pictured = make("a[img=x.png]b[img=x.png width=40 valign=baseline]c", options);
    EXPECT_TRUE(pictured.getLayout().waitingForImages);
    EXPECT_TRUE(pictured.getLayout().images.empty());
    EXPECT_EQ(pictured.getCharacterCount(), 5U);

    loaded = true;
    pictured.update(0.0F);
    const RichTextLayout& laid = pictured.getLayout();
    ASSERT_EQ(laid.images.size(), 2U);
    EXPECT_FALSE(laid.waitingForImages);
    EXPECT_EQ(laid.images[0].rect.getSize(), math::Vec2(20.0F, 10.0F));
    EXPECT_EQ(laid.images[1].rect.getSize(), math::Vec2(40.0F, 20.0F));
    EXPECT_FLOAT_EQ(laid.images[1].rect.getBottom(), glyphOf(laid, U'c').baseline);
    EXPECT_LT(laid.images[0].rect.getBottom(), glyphOf(laid, U'b').baseline);
    EXPECT_FLOAT_EQ(laid.characters[2].box.x, laid.images[0].rect.getRight());

    registry()->registerIcon("jump", {.texture = wide});
    RichText iconic = make("press [icon=jump] now", {.size = 30.0F});
    ASSERT_EQ(iconic.getLayout().images.size(), 1U);
    EXPECT_EQ(iconic.getLayout().images[0].rect.getSize(), math::Vec2(60.0F, 30.0F));
    EXPECT_THROW((void)make("[icon=missing]").getLayout(), std::invalid_argument);
    EXPECT_THROW((void)make("[img=a.png]").getLayout(), std::invalid_argument);
}

TEST_F(RichTextLayoutTest, HitTestsLinksAndHints) {
    RichText linked = make("go [url=home]to the home page[/url] or [hint=A tip]here[/hint]", {.size = 20.0F, .maxWidth = 120.0F});
    const RichTextLayout& laid = linked.getLayout();
    ASSERT_GE(laid.links.size(), 2U);
    for (const RichTextLayout::Area& area : laid.links) {
        EXPECT_EQ(linked.getLinkAt(area.rect.getCenter()), "home");
    }
    EXPECT_FALSE(linked.getLinkAt({1.0F, 1.0F}));
    ASSERT_EQ(laid.hints.size(), 1U);
    EXPECT_EQ(linked.getHintAt(laid.hints[0].rect.getCenter()), "A tip");
    EXPECT_FALSE(linked.getHintAt(laid.links[0].rect.getCenter()));

    const auto underlines = std::count_if(laid.boxes.begin(), laid.boxes.end(), [](const RichTextLayout::Box& box) { return box.kind == RichTextLayout::Box::Kind::Underline; });
    EXPECT_EQ(static_cast<std::size_t>(underlines), laid.links.size());
    RichText plain = make("[url=x]link[/url]", {.underlineLinks = false});
    EXPECT_TRUE(plain.getLayout().boxes.empty());
}

TEST_F(RichTextLayoutTest, RunsEffectsDeterministically) {
    RichText first = make("[wave amp=50 freq=2]wave[/wave] still");
    RichText second = make("[wave amp=50 freq=2]wave[/wave] still");
    first.update(0.25F);
    second.update(0.1F);
    second.update(0.15F);
    const RichTextLayout& laid = first.getLayout();
    const RichTextLayout& moved = first.getFrame();
    for (std::size_t index = 0; index < moved.glyphs.size(); ++index) {
        EXPECT_NEAR(moved.glyphs[index].position.y, second.getFrame().glyphs[index].position.y, 0.0001F);
    }
    const RichTextLayout::Glyph& w = glyphOf(laid, U'w');
    EXPECT_NEAR(glyphOf(moved, U'w').position.y - w.position.y, std::sin(2.0F * 0.25F + w.position.x / 50.0F) * 5.0F, 0.001F);
    EXPECT_FLOAT_EQ(glyphOf(moved, U's').position.y, glyphOf(laid, U's').position.y);

    registry()->registerEffect("everyOther", [](TextEffect::Glyph& glyph, const TextEffect::Parameters& parameters) { glyph.visible = glyph.index % static_cast<std::size_t>(parameters.getNumber("step", 2.0F)) == 0; });
    RichText skipping = make("[everyOther]abcd[/everyOther][fade start=0 length=4]wxyz[/fade]");
    const RichTextLayout& skipped = skipping.getFrame();
    EXPECT_TRUE(glyphOf(skipped, U'a').visible);
    EXPECT_FALSE(glyphOf(skipped, U'b').visible);
    EXPECT_TRUE(glyphOf(skipped, U'c').visible);
    EXPECT_FLOAT_EQ(glyphOf(skipped, U'w').color.a, 1.0F);
    EXPECT_FLOAT_EQ(glyphOf(skipped, U'y').color.a, 0.5F);

    for (const char* effect : {"shake", "tornado", "rainbow", "pulse"}) {
        RichText animated = make(std::string("[") + effect + "]ab[/" + effect + "]");
        animated.update(0.3F);
        const RichTextLayout& frame = animated.getFrame();
        const bool changed = frame.glyphs[0].position != animated.getLayout().glyphs[0].position || frame.glyphs[0].color != animated.getLayout().glyphs[0].color;
        EXPECT_TRUE(changed) << effect;
    }

    try {
        (void)make("ok\n  [nope]x[/nope]");
        FAIL() << "An unregistered effect must be reported.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "Rich text markup at line 2, column 3: [nope] is neither a tag nor a registered text effect.");
    }
    RichText broken = make("[wave amp=loud]x[/wave]");
    EXPECT_THROW((void)broken.getFrame(), std::invalid_argument);
    EXPECT_THROW(registry()->registerEffect("wave", [](TextEffect::Glyph&, const TextEffect::Parameters&) {}), std::invalid_argument);
    EXPECT_THROW(registry()->registerEffect("b", [](TextEffect::Glyph&, const TextEffect::Parameters&) {}), std::invalid_argument);
}

TEST_F(RichTextLayoutTest, RevealsLikeATypewriter) {
    RichText typed = make("ab[pause=1]cd [speed=2]ef[/speed]", {.revealSpeed = 10.0F});
    EXPECT_EQ(typed.getVisibleCharacters(), 0U);
    EXPECT_TRUE(typed.isRevealing());
    typed.update(0.25F);
    EXPECT_EQ(typed.getVisibleCharacters(), 2U);
    typed.update(1.0F);
    EXPECT_EQ(typed.getVisibleCharacters(), 2U);
    typed.update(0.1F);
    EXPECT_EQ(typed.getVisibleCharacters(), 3U);

    // The last two characters take half the time each.
    typed.update(0.22F);
    EXPECT_EQ(typed.getVisibleCharacters(), 6U);
    typed.update(0.1F);
    EXPECT_EQ(typed.getVisibleCharacters(), 7U);
    EXPECT_FALSE(typed.isRevealing());

    typed.setVisibleCharacters(1);
    EXPECT_TRUE(glyphOf(typed.getFrame(), U'a').visible);
    EXPECT_FALSE(glyphOf(typed.getFrame(), U'b').visible);
    typed.update(0.1F);
    EXPECT_EQ(typed.getVisibleCharacters(), 2U);
    typed.setVisibleRatio(0.5F);
    EXPECT_EQ(typed.getVisibleCharacters(), 4U);
    EXPECT_FLOAT_EQ(typed.getVisibleRatio(), 4.0F / 7.0F);

    // Backgrounds and lines follow the revealed text.
    RichText underlined = make("[u]abcdef[/u]");
    underlined.setVisibleCharacters(3);
    const RichTextLayout& partial = underlined.getFrame();
    ASSERT_EQ(partial.boxes.size(), 1U);
    EXPECT_NEAR(partial.boxes[0].rect.getRight(), partial.characters[2].box.getRight(), 0.001F);
    EXPECT_LT(partial.boxes[0].rect.width, underlined.getLayout().boxes[0].rect.width);
    underlined.setVisibleCharacters(0);
    EXPECT_FALSE(underlined.getFrame().boxes[0].visible);
    underlined.setVisibleCharacters(100);
    EXPECT_EQ(&underlined.getFrame(), &underlined.getLayout());
    EXPECT_THROW(underlined.update(-1.0F), std::invalid_argument);
}

TEST_F(RichTextLayoutTest, SelectsFallbackGlyphsAndSynthesizesStyles) {
    const std::shared_ptr<Font> grid = gridFont(fixture.engine());
    const std::shared_ptr<Font> bold = copyOfDefaultFont(fixture.engine());
    const auto family = std::make_shared<FontFamily>(FontFamily::Faces{.regular = registry()->getDefaultFamily()->getFaces().regular, .fallbacks = {grid}});
    RichText mixed = make("a\U0000E000 [b]b[/b][i]i[/i][b][i]x[/i][/b]", {.family = family, .size = 16.0F});
    const RichTextLayout& laid = mixed.getLayout();
    EXPECT_EQ(laid.looks[glyphOf(laid, U'\U0000E000').look].font, grid.get());
    EXPECT_EQ(laid.looks[glyphOf(laid, U'a').look].font, family->getFaces().regular.get());

    const RichTextLayout::Look& synthesizedBold = laid.looks[glyphOf(laid, U'b').look];
    EXPECT_NEAR(synthesizedBold.weight, 16.0F * 0.03F, 0.0001F);
    EXPECT_FLOAT_EQ(synthesizedBold.skew, 0.0F);
    EXPECT_FLOAT_EQ(laid.looks[glyphOf(laid, U'i').look].skew, 0.2F);
    EXPECT_GT(laid.looks[glyphOf(laid, U'x').look].weight, 0.0F);
    EXPECT_GT(laid.looks[glyphOf(laid, U'x').look].skew, 0.0F);

    const auto withBold = std::make_shared<FontFamily>(FontFamily::Faces{.regular = family->getFaces().regular, .bold = bold});
    RichText real = make("[b]b[/b][b][i]x[/i][/b][code]c[/code]", {.family = withBold});
    const RichTextLayout& faces = real.getLayout();
    EXPECT_EQ(faces.looks[glyphOf(faces, U'b').look].font, bold.get());
    EXPECT_FLOAT_EQ(faces.looks[glyphOf(faces, U'b').look].weight, 0.0F);
    EXPECT_EQ(faces.looks[glyphOf(faces, U'x').look].font, bold.get());
    EXPECT_FLOAT_EQ(faces.looks[glyphOf(faces, U'x').look].skew, 0.2F);
    EXPECT_EQ(faces.looks[glyphOf(faces, U'c').look].font, family->getFaces().regular.get());

    // A bitmap font cannot move its edge, so its synthetic bold draws twice a native pixel apart.
    RichText pixel = make("[b]A[/b]", {.family = std::make_shared<FontFamily>(FontFamily::Faces{.regular = grid}), .size = 16.0F});
    const RichTextLayout::Look& embolden = pixel.getLayout().looks[pixel.getLayout().glyphs[0].look];
    EXPECT_FLOAT_EQ(embolden.weight, 0.0F);
    EXPECT_FLOAT_EQ(embolden.emboldenOffset, 2.0F);

    RichText named = make("[font=pixel]A[/font]", {.fonts = [&](std::string_view name) { return name == "pixel" ? std::make_shared<FontFamily>(FontFamily::Faces{.regular = grid}) : nullptr; }});
    EXPECT_EQ(named.getLayout().looks[named.getLayout().glyphs[0].look].font, grid.get());
    EXPECT_THROW((void)make("[font=other]A[/font]").getLayout(), std::invalid_argument);
    EXPECT_THROW(FontFamily(FontFamily::Faces{}), std::invalid_argument);
}

} // namespace haylen::text
