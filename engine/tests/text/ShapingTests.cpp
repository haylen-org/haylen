#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Utf8.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/RichText.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "support/EngineFixture.hpp"
#include "text/BidiParagraph.hpp"

namespace haylen::text {

namespace {

// Lays out text in the default font with the test fonts as fallbacks for Arabic, Hebrew, Devanagari, Thai and Japanese.
class ShapingTest : public ::testing::Test {
  protected:
    ShapingTest() {
        arabic = load("noto_sans_arabic_regular.ttf");
        hebrew = load("noto_sans_hebrew_regular.ttf");
        devanagari = load("noto_sans_devanagari_regular.ttf");
        thai = load("noto_sans_thai_regular.ttf");
        japanese = load("mplus_1p_regular.ttf");
        family = std::make_shared<FontFamily>(FontFamily::Faces{.regular = fixture.engine().getDefaultFont(), .fallbacks = {arabic, hebrew, devanagari, thai, japanese}});
    }

    [[nodiscard]] std::shared_ptr<Font> load(std::string_view name) {
        std::ifstream file(std::string(HAYLEN_TEST_FONTS) + "/" + std::string(name), std::ios::binary);
        return std::make_shared<TrueTypeFont>(fixture.engine().getGraphics(), std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()));
    }

    // Shapes one code point on its own, which gives its isolated form, and returns the glyphs it draws with.
    [[nodiscard]] static std::vector<std::uint32_t> isolated(Font& font, char32_t codePoint, std::uint32_t script) {
        std::vector<Font::ShapedGlyph> shaped;
        const std::u32string text(1, codePoint);
        font.shape({.text = text, .begin = 0, .end = 1, .script = script}, shaped);
        return clusterOf(shaped, 0);
    }

    // Returns the sorted glyphs of one cluster of shaped text, which in fonts that draw the dots of Arabic letters apart holds the letter and its dots.
    [[nodiscard]] static std::vector<std::uint32_t> clusterOf(const std::vector<Font::ShapedGlyph>& shaped, std::size_t cluster) {
        std::vector<std::uint32_t> indices;
        for (const Font::ShapedGlyph& glyph : shaped) {
            if (glyph.cluster == cluster) {
                indices.push_back(glyph.index);
            }
        }
        std::ranges::sort(indices);
        return indices;
    }

    [[nodiscard]] const Layout& lay(std::string_view text, Style style = {.size = 32.0F}) {
        laid = family->layout(text, style);
        return *laid;
    }

    // Returns the glyph drawn for the character that starts at a code point of the text.
    [[nodiscard]] static const Layout::Glyph& glyphAt(const Layout& layout, std::size_t codePoint) {
        const auto character = std::ranges::find_if(layout.characters, [codePoint](const Layout::Character& found) { return found.begin <= codePoint && codePoint < found.end; });
        const auto glyph = std::ranges::find_if(layout.glyphs, [&](const Layout::Glyph& found) { return found.character == static_cast<std::size_t>(character - layout.characters.begin()); });
        EXPECT_NE(glyph, layout.glyphs.end()) << codePoint;
        return *glyph;
    }

    static constexpr std::uint32_t kArab = 0x41726162U;
    static constexpr std::uint32_t kDeva = 0x44657661U;

    test::EngineFixture fixture;
    std::shared_ptr<Font> arabic;
    std::shared_ptr<Font> hebrew;
    std::shared_ptr<Font> devanagari;
    std::shared_ptr<Font> thai;
    std::shared_ptr<Font> japanese;
    std::shared_ptr<FontFamily> family;
    std::shared_ptr<const Layout> laid;
};

} // namespace

// Arabic letters join, so each one takes the form of its place in the word, and lam followed by alef becomes one ligature.
TEST_F(ShapingTest, JoinsArabicLettersAndFormsTheLamAlefLigature) {
    const std::u32string word = U"بيت";
    std::vector<Font::ShapedGlyph> shaped;
    arabic->shape({.text = word, .begin = 0, .end = word.size(), .script = kArab, .rightToLeft = true}, shaped);
    EXPECT_EQ(shaped.front().cluster, 2U);
    EXPECT_EQ(shaped.back().cluster, 0U);
    for (std::size_t index = 0; index < 3; ++index) {
        EXPECT_NE(clusterOf(shaped, index), isolated(*arabic, word[index], kArab)) << index;
    }

    // Lam followed by alef takes the forms of the lam-alef ligature, which this font draws as two glyphs of their own, unlike the forms the same letters take next to other letters.
    // clang-format off
    const auto formsOf = [&](std::string_view text) {
        const Layout& layout = lay(text);
        std::vector<std::uint32_t> forms;
        for (const Layout::Glyph& glyph : layout.glyphs) {
            EXPECT_EQ(layout.looks[glyph.look].font, arabic.get());
            EXPECT_TRUE(layout.characters[glyph.character].rightToLeft);
            forms.push_back(glyph.index);
        }
        return forms;
    };
    // clang-format on
    const std::vector<std::uint32_t> lamAlef = formsOf("لا");
    const std::vector<std::uint32_t> lamMeem = formsOf("لم");
    const std::vector<std::uint32_t> behAlef = formsOf("با");
    ASSERT_EQ(lamAlef.size(), 2U);
    EXPECT_EQ(std::ranges::count(lamMeem, lamAlef[1]), 0);
    EXPECT_EQ(std::ranges::count(behAlef, lamAlef[0]), 0);
    EXPECT_EQ(std::ranges::count(isolated(*arabic, U'ل', kArab), lamAlef[1]), 0);

    // A letter whose neighbour sits in another style still joins it, since every run shapes with the whole paragraph around it.
    RichText split("[b]ب[/b]يت", {.family = family, .size = 32.0F}, fixture.engine().getPlugin<plugins::TextPlugin>().getRegistry());
    std::vector<std::uint32_t> joined;
    for (const Layout::Glyph& glyph : split.getLayout().glyphs) {
        if (glyph.character == 0) {
            joined.push_back(glyph.index);
        }
    }
    std::ranges::sort(joined);
    EXPECT_NE(joined, isolated(*arabic, U'ب', kArab));
}

// A paragraph reads in the direction of its first strong letter, and numbers and Latin words inside a right-to-left paragraph keep their own left-to-right order.
TEST_F(ShapingTest, OrdersMixedDirectionsForDisplay) {
    const Layout& hebrewFirst = lay("שלום 123 abc");
    EXPECT_TRUE(hebrewFirst.lines[0].rightToLeft);
    const float shin = glyphAt(hebrewFirst, 0).position.x;
    const float mem = glyphAt(hebrewFirst, 3).position.x;
    const float one = glyphAt(hebrewFirst, 5).position.x;
    const float three = glyphAt(hebrewFirst, 7).position.x;
    const float a = glyphAt(hebrewFirst, 9).position.x;
    const float c = glyphAt(hebrewFirst, 11).position.x;
    EXPECT_GT(shin, mem);
    EXPECT_GT(mem, three);
    EXPECT_LT(one, three);
    EXPECT_GT(one, c);
    EXPECT_LT(a, c);

    // Arabic with an English word and a number, forced to read left to right, keeps the English on the left and reverses the Arabic word, and the number after it joins the Arabic run as an Arabic number, so it stands on the left of the word with its digits in order.
    const Layout& arabicInside = lay("Price السعر 42", {.size = 32.0F, .direction = Direction::LeftToRight});
    EXPECT_FALSE(arabicInside.lines[0].rightToLeft);
    const auto boxOf = [&](std::size_t codePoint) { return std::ranges::find_if(arabicInside.characters, [codePoint](const Layout::Character& found) { return found.begin <= codePoint && codePoint < found.end; })->box.x; };
    EXPECT_LT(boxOf(0), boxOf(12));
    EXPECT_LT(boxOf(12), boxOf(13));
    EXPECT_LT(boxOf(13), boxOf(10));
    EXPECT_LT(boxOf(10), boxOf(6));

    // Start follows the direction, so a right-to-left paragraph lines up on the right, and brackets mirror inside it.
    const Layout& aligned = lay("שלום (עולם)", {.size = 32.0F, .maxWidth = 600.0F});
    EXPECT_NEAR(aligned.lines[0].box.getRight(), 600.0F, 1.0F);
    const Layout& left = lay("שלום", {.size = 32.0F, .align = Alignment::Left, .maxWidth = 600.0F});
    EXPECT_NEAR(left.lines[0].box.x, 0.0F, 0.01F);
    const std::u32string brackets = U"(א)";
    std::vector<Font::ShapedGlyph> mirrored;
    hebrew->shape({.text = brackets, .begin = 0, .end = 3, .script = 0x48656272U, .rightToLeft = true}, mirrored);
    std::vector<Font::ShapedGlyph> upright;
    hebrew->shape({.text = brackets, .begin = 0, .end = 3, .script = 0x48656272U}, upright);
    EXPECT_EQ(mirrored.back().index, upright.front().index);
}

// Devanagari joins consonants through the virama into conjuncts, draws the vowel sign i before the consonant it follows, and hangs marks such as the anusvara over their letter.
TEST_F(ShapingTest, FormsDevanagariConjunctsAndPlacesMarks) {
    const Layout& conjunct = lay("क्ष");
    ASSERT_EQ(conjunct.characters.size(), 1U);
    EXPECT_EQ(conjunct.characters[0].end, 3U);
    EXPECT_LT(conjunct.glyphs.size(), 3U);
    EXPECT_EQ(conjunct.looks[conjunct.glyphs[0].look].font, devanagari.get());

    const Layout& vowel = lay("कि");
    ASSERT_EQ(vowel.characters.size(), 1U);
    ASSERT_EQ(vowel.glyphs.size(), 2U);
    const std::uint32_t ka = isolated(*devanagari, U'क', kDeva).front();
    const auto consonant = std::ranges::find(vowel.glyphs, ka, &Layout::Glyph::index);
    ASSERT_NE(consonant, vowel.glyphs.end());
    const auto sign = consonant == vowel.glyphs.begin() ? vowel.glyphs.end() - 1 : vowel.glyphs.begin();
    EXPECT_LT(sign->position.x, consonant->position.x);

    const Layout& marked = lay("कं");
    ASSERT_EQ(marked.characters.size(), 1U);
    ASSERT_EQ(marked.glyphs.size(), 2U);
    EXPECT_LT(marked.glyphs[1].position.y + marked.glyphs[1].size.y, marked.glyphs[0].position.y + marked.glyphs[0].size.y * 0.5F);
    EXPECT_FLOAT_EQ(marked.characters[0].box.width, family->measure("क", {.size = 32.0F}).x);
}

// Every paragraph separator ends a paragraph, and the carriage return of CRLF stays at the end of its paragraph, so the characters of plain text keep counting its code points.
TEST_F(ShapingTest, EndsPlainParagraphsAtEverySeparator) {
    const Layout& paragraphs = lay("a\rb\u2029c\u0085d\r\ne");
    ASSERT_EQ(paragraphs.lines.size(), 5U);
    EXPECT_EQ(paragraphs.characters.back().begin, 9U);
    EXPECT_EQ(paragraphs.lines[3].end, 8U);
    EXPECT_THROW(BidiParagraph(U"a\u2029b", Direction::Auto), std::invalid_argument);
}

// Lines wrap where the Unicode rules allow in every script: at spaces in Latin, Arabic, Hebrew and Devanagari, between ideographs in Japanese, and never inside a cluster of Thai, which has no spaces.
TEST_F(ShapingTest, BreaksLinesByTheRulesOfEachScript) {
    // clang-format off
    const auto startsAfterSpace = [](const Layout& layout, std::u32string_view text) {
        for (std::size_t index = 1; index < layout.lines.size(); ++index) {
            const std::size_t first = layout.characters[layout.lines[index].firstCharacter].begin;
            if (first == 0 || text[first - 1] != U' ') {
                return false;
            }
        }
        return layout.lines.size() > 1;
    };
    // clang-format on
    for (const std::u32string& text : {std::u32string(U"one two three four five six"), std::u32string(U"مرحبا بك في عالم الألعاب الجميل"), std::u32string(U"שלום לכם וברוכים הבאים לעולם"), std::u32string(U"नमस्ते और खेल की दुनिया में स्वागत है")}) {
        std::string utf8;
        for (const char32_t codePoint : text) {
            core::Utf8::append(utf8, codePoint);
        }
        EXPECT_TRUE(startsAfterSpace(lay(utf8, {.size = 32.0F, .maxWidth = 220.0F}), text)) << utf8;
    }

    // Right-to-left lines each end at the right edge they start from.
    const Layout& arabicLines = lay("مرحبا بك في عالم الألعاب الجميل", {.size = 32.0F, .maxWidth = 220.0F});
    for (const Layout::Line& line : arabicLines.lines) {
        EXPECT_TRUE(line.rightToLeft);
        EXPECT_NEAR(line.box.getRight(), 220.0F, 1.0F);
    }

    const Layout& wrapped = lay("日本語のテキストを折り返す", {.size = 32.0F, .maxWidth = 100.0F, .language = "ja"});
    EXPECT_GT(wrapped.lines.size(), 2U);

    // Thai lines break between words where it can, and a word too long for a line breaks between its clusters, never between a letter and its vowel sign.
    const std::string sentence = "ภาษาไทยไม่มีช่องว่างระหว่างคำเลยสักนิดเดียว";
    const Layout& thaiLines = lay(sentence, {.size = 32.0F, .maxWidth = 160.0F, .language = "th"});
    ASSERT_GT(thaiLines.lines.size(), 1U);
    const std::u32string decoded = core::Utf8::decode(sentence);
    for (std::size_t index = 1; index < thaiLines.lines.size(); ++index) {
        const char32_t first = decoded[thaiLines.characters[thaiLines.lines[index].firstCharacter].begin];
        EXPECT_FALSE(first >= U'\U00000E31' && first <= U'\U00000E3A') << index;
        EXPECT_FALSE(first >= U'\U00000E47' && first <= U'\U00000E4E') << index;
    }
}

// Styles apply across a change of direction, and backgrounds and lines split where it changes so each grows with the reveal from its own start.
TEST_F(ShapingTest, SpansStylesAcrossDirections) {
    const std::shared_ptr<RichTextRegistry> registry = fixture.engine().getPlugin<plugins::TextPlugin>().getRegistry();
    RichText mixed("[u]שלום abc[/u] [color=red]مرحبا[/color]", {.family = family, .size = 32.0F}, registry);
    const Layout& layout = mixed.getLayout();
    std::vector<const Layout::Box*> underlines;
    for (const Layout::Box& box : layout.boxes) {
        if (box.kind == Layout::Box::Kind::Underline) {
            underlines.push_back(&box);
        }
    }
    ASSERT_EQ(underlines.size(), 2U);
    EXPECT_NE(underlines[0]->rightToLeft, underlines[1]->rightToLeft);
    const auto red = std::ranges::count_if(layout.glyphs, [](const Layout::Glyph& glyph) { return glyph.color == math::Color::fromHex(0xFF0000FFU); });
    EXPECT_GE(red, 4);

    // A right-to-left paragraph of markup lines up on the right and puts its list markers there.
    RichText list("[p dir=rightToLeft][ul]אחד[/ul][/p]", {.family = family, .size = 32.0F, .maxWidth = 400.0F}, registry);
    const Layout& items = list.getLayout();
    ASSERT_FALSE(items.glyphs.empty());
    const Layout::Glyph& bullet = *std::ranges::find(items.glyphs, U'•', &Layout::Glyph::codePoint);
    EXPECT_GT(bullet.position.x, items.characters[0].box.getRight());
}

// The typewriter reveals characters in reading order, which starts at the right of right-to-left text, and a partly revealed underline grows from there.
TEST_F(ShapingTest, RevealsInReadingOrder) {
    const std::shared_ptr<RichTextRegistry> registry = fixture.engine().getPlugin<plugins::TextPlugin>().getRegistry();
    RichText typed("[u]سلام دنیا[/u]", {.family = family, .size = 32.0F, .revealSpeed = 10.0F}, registry);
    typed.setVisibleCharacters(2);
    const Layout& frame = typed.getFrame();
    ASSERT_GT(frame.characters.size(), 3U);
    EXPECT_GT(frame.characters[0].box.x, frame.characters[1].box.x);
    EXPECT_GT(frame.characters[1].box.x, frame.characters[2].box.x);
    for (const Layout::Glyph& glyph : frame.glyphs) {
        EXPECT_EQ(glyph.visible, glyph.character < 2) << glyph.character;
    }
    ASSERT_EQ(frame.boxes.size(), 1U);
    EXPECT_NEAR(frame.boxes[0].rect.getRight(), typed.getLayout().boxes[0].rect.getRight(), 0.01F);
    EXPECT_NEAR(frame.boxes[0].rect.x, frame.characters[1].box.x, 0.01F);
}

// Plain layouts are cached by text and by the style fields that change them, so the same text in another color lays out once.
TEST_F(ShapingTest, CachesLayoutsByTextAndStyle) {
    const std::shared_ptr<const Layout> first = family->layout("مرحبا world", {.size = 24.0F});
    EXPECT_EQ(family->layout("مرحبا world", {.size = 24.0F, .color = math::Color::black(), .rotation = 1.0F}), first);
    EXPECT_NE(family->layout("مرحبا world", {.size = 24.0F, .maxWidth = 50.0F}), first);
    EXPECT_NE(family->layout("مرحبا world", {.size = 24.0F, .direction = Direction::LeftToRight}), first);
    EXPECT_NE(family->layout("مرحبا world", {.size = 24.0F, .language = "fa"}), first);

    Font& font = *fixture.engine().getDefaultFont();
    const std::shared_ptr<const Layout> plain = font.layout("cached", {.size = 24.0F});
    EXPECT_EQ(font.layout("cached", {.size = 24.0F}), plain);
    for (int index = 0; index < 600; ++index) {
        (void)font.layout(std::to_string(index), {.size = 24.0F});
    }
    EXPECT_NE(font.layout("cached", {.size = 24.0F}), plain);
}

} // namespace haylen::text
