#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/text/RichText.hpp"
#include "haylen/text/RichTextDocument.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::text {

namespace {

using Kind = RichTextDocument::Inline::Kind;

class RichTextMarkupTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::string errorOf(const std::string& markup) {
        try {
            (void)RichText::parse(markup);
        } catch (const std::invalid_argument& error) {
            return error.what();
        }
        return "no error";
    }
};

} // namespace

// Paragraph markup reads the alignment and direction names that Lua and UI documents use, from the one table the text style keeps.
TEST_F(RichTextMarkupTest, ReadsTheAlignmentAndDirectionNamesOfTheTextStyle) {
    for (const auto& [name, alignment] : Style::kAlignmentNames) {
        EXPECT_EQ(RichText::parse("[p align=" + std::string(name) + "]x[/p]").paragraphs[0].align, alignment);
        EXPECT_EQ(Style::alignmentName(alignment), name);
    }
    for (const auto& [name, direction] : Style::kDirectionNames) {
        EXPECT_EQ(RichText::parse("[p dir=" + std::string(name) + "]x[/p]").paragraphs[0].direction, direction);
        EXPECT_EQ(Style::directionName(direction), name);
    }
    EXPECT_FALSE(Style::alignmentFromName("justify").has_value());
    EXPECT_FALSE(Style::directionFromName("up").has_value());
    EXPECT_NE(errorOf("[p dir=up]x[/p]").find("The \"dir\" of \"[p]\" must be \"auto\", \"leftToRight\" or \"rightToLeft\"."), std::string::npos);
}

TEST_F(RichTextMarkupTest, NestsInlineStylesAndMergesRuns) {
    const RichTextDocument document = RichText::parse("plain [b]bold [i]both[/i][/b] [color=#FF0000][u]red[/u][/color] [size=150%][s]big[/s][/size]");
    ASSERT_EQ(document.paragraphs.size(), 1U);
    const auto& inlines = document.paragraphs[0].inlines;
    ASSERT_EQ(inlines.size(), 7U);
    EXPECT_EQ(inlines[0].text, U"plain ");
    EXPECT_FALSE(document.styles[inlines[0].style].bold);
    EXPECT_TRUE(document.styles[inlines[1].style].bold);
    EXPECT_FALSE(document.styles[inlines[1].style].italic);
    EXPECT_TRUE(document.styles[inlines[2].style].bold);
    EXPECT_TRUE(document.styles[inlines[2].style].italic);

    const RichTextDocument::Style& red = document.styles[inlines[4].style];
    EXPECT_EQ(inlines[4].text, U"red");
    EXPECT_TRUE(red.underline);
    EXPECT_EQ(red.color, math::Color::fromHex(0xFF0000FFU));
    EXPECT_FLOAT_EQ(document.styles[inlines[6].style].sizeFactor, 1.5F);
    EXPECT_TRUE(document.styles[inlines[6].style].strike);

    // Closing a tag returns to the very style before it, so equal neighbours share one run.
    const RichTextDocument merged = RichText::parse("a[b][/b]b");
    ASSERT_EQ(merged.paragraphs[0].inlines.size(), 1U);
    EXPECT_EQ(merged.paragraphs[0].inlines[0].text, U"ab");
}

TEST_F(RichTextMarkupTest, ReadsStylesWithAttributes) {
    const RichTextDocument document = RichText::parse("[outline=3 color=blue][shadow=2,4 color=#80000000 blur=3][glow=5 color=gold][alpha=0.5][alpha=0.5][font=title][size=20][code][bgcolor=yellow]x[/bgcolor][/code][/size][/font][/alpha][/alpha][/glow][/shadow][/outline]");
    const RichTextDocument::Style& style = document.styles[document.paragraphs[0].inlines[0].style];
    EXPECT_FLOAT_EQ(style.outlineWidth, 3.0F);
    EXPECT_EQ(style.outlineColor, math::Color::fromHex(0x0000FFFFU));
    ASSERT_TRUE(style.shadow);
    EXPECT_EQ(style.shadow->offset, math::Vec2(2.0F, 4.0F));
    EXPECT_FLOAT_EQ(style.shadow->blur, 3.0F);
    EXPECT_NEAR(style.shadow->color.a, 0.5F, 0.01F);
    ASSERT_TRUE(style.glow);
    EXPECT_FLOAT_EQ(style.glow->width, 5.0F);
    EXPECT_FLOAT_EQ(style.alpha, 0.25F);
    EXPECT_EQ(style.font, "title");
    EXPECT_EQ(style.size, 20.0F);
    EXPECT_TRUE(style.mono);
    EXPECT_EQ(style.background, math::Color::fromHex(0xFFFF00FFU));
}

TEST_F(RichTextMarkupTest, EscapesBracketsAndSplitsParagraphs) {
    const RichTextDocument document = RichText::parse("[lb]b[rb] is not bold]\nsecond[br]line\n");
    ASSERT_EQ(document.paragraphs.size(), 3U);
    EXPECT_EQ(document.paragraphs[0].inlines[0].text, U"[b] is not bold]");
    ASSERT_EQ(document.paragraphs[1].inlines.size(), 3U);
    EXPECT_EQ(document.paragraphs[1].inlines[1].kind, Kind::LineBreak);
    EXPECT_TRUE(document.paragraphs[2].inlines.empty());

    // The newline right after a block tag and right before its end belongs to the markup.
    const RichTextDocument centered = RichText::parse("intro\n[center]\ntitle\n[/center]\nafter");
    ASSERT_EQ(centered.paragraphs.size(), 3U);
    EXPECT_FALSE(centered.paragraphs[0].align);
    EXPECT_EQ(centered.paragraphs[1].align, Alignment::Center);
    EXPECT_EQ(centered.paragraphs[1].inlines[0].text, U"title");
    EXPECT_EQ(centered.paragraphs[2].inlines[0].text, U"after");
    EXPECT_EQ(RichText::parse("").paragraphs.size(), 1U);
}

TEST_F(RichTextMarkupTest, NumbersListsAndIndentsBlocks) {
    const RichTextDocument document = RichText::parse("[ol type=I]\none\ntwo\n[ol type=a]\nsub\n[/ol]\nthree\n[/ol][ul bullet=*]dot[/ul][p align=fill indent=2]para[/p][right]r[/right][ol type=A]x\n\ny[/ol]");
    const auto& paragraphs = document.paragraphs;
    ASSERT_EQ(paragraphs.size(), 10U);
    EXPECT_EQ(paragraphs[0].marker, U"I.");
    EXPECT_EQ(paragraphs[1].marker, U"II.");
    EXPECT_EQ(paragraphs[2].marker, U"a.");
    EXPECT_FLOAT_EQ(paragraphs[2].indent, 2.0F);
    EXPECT_EQ(paragraphs[3].marker, U"III.");
    EXPECT_EQ(paragraphs[4].marker, U"*");
    EXPECT_EQ(paragraphs[5].align, Alignment::Fill);
    EXPECT_FLOAT_EQ(paragraphs[5].indent, 2.0F);
    EXPECT_EQ(paragraphs[6].align, Alignment::Right);

    // A blank line inside a list takes no number.
    EXPECT_EQ(paragraphs[7].marker, U"A.");
    EXPECT_TRUE(paragraphs[8].marker.empty());
    EXPECT_EQ(paragraphs[9].marker, U"B.");
    EXPECT_EQ(RichText::parse("[ol type=i]a\nb\nc\nd[/ol]").paragraphs[3].marker, U"iv.");
    EXPECT_EQ(RichText::parse("[ol]" + std::string(27, 'x') + "[/ol]").paragraphs[0].marker, U"1.");
}

TEST_F(RichTextMarkupTest, ReadsObjectsLinksHintsAndRevealTags) {
    const RichTextDocument document = RichText::parse("[url]https://haylen.dev[/url] [url=\"shop item\"]buy[/url] [hint=Costs 5 coins]price[/hint][img=icons/coin.png width=24 region=0,0,16,16 color=#80FFFFFF valign=top][icon=jump height=30][pause=0.5][speed=2]fast[/speed][hr width=50% height=4 color=red]");
    ASSERT_EQ(document.links.size(), 2U);
    EXPECT_EQ(document.links[0], "https://haylen.dev");
    EXPECT_EQ(document.links[1], "shop item");
    EXPECT_EQ(document.hints[0], "Costs 5 coins");

    ASSERT_EQ(document.images.size(), 1U);
    EXPECT_EQ(document.images[0].path, "icons/coin.png");
    EXPECT_EQ(document.images[0].width, 24.0F);
    EXPECT_FALSE(document.images[0].height);
    EXPECT_EQ(document.images[0].region, math::Rect(0.0F, 0.0F, 16.0F, 16.0F));
    EXPECT_EQ(document.images[0].align, RichTextDocument::VerticalAlign::Top);
    EXPECT_EQ(document.icons[0].name, "jump");
    EXPECT_EQ(document.icons[0].height, 30.0F);

    const auto& inlines = document.paragraphs[0].inlines;
    const auto pause = std::find_if(inlines.begin(), inlines.end(), [](const RichTextDocument::Inline& item) { return item.kind == Kind::Pause; });
    ASSERT_NE(pause, inlines.end());
    EXPECT_FLOAT_EQ(pause->seconds, 0.5F);
    EXPECT_FLOAT_EQ(document.styles[inlines.back().style].revealSpeed, 2.0F);

    const RichTextDocument::Paragraph& rule = document.paragraphs.back();
    EXPECT_EQ(rule.kind, RichTextDocument::Paragraph::Kind::Rule);
    EXPECT_FLOAT_EQ(rule.ruleWidth, 0.5F);
    EXPECT_FLOAT_EQ(rule.ruleThickness, 4.0F);
}

TEST_F(RichTextMarkupTest, RecordsEffectsAndBuildsTablesAndDropCaps) {
    const RichTextDocument document = RichText::parse("[wave amp=40][sparkle=3]x[/sparkle][/wave]\n[table=2]\n[cell bg=#202020 border=white padding=6]a[/cell][cell]b\nc[/cell]\n[cell][b]d[/b][/cell]\n[/table]\n[dropcap size=64 color=red margin=8]O[/dropcap]nce upon a time");
    ASSERT_EQ(document.effects.size(), 2U);
    EXPECT_EQ(document.effects[0].name, "wave");
    EXPECT_EQ(document.effects[0].parameters.at("amp"), "40");
    EXPECT_EQ(document.effects[1].parameters.at("value"), "3");
    EXPECT_EQ(document.effects[1].column, 14U);
    EXPECT_EQ(document.styles[document.paragraphs[0].inlines[0].style].effects, (std::vector<std::size_t>{0, 1}));

    ASSERT_EQ(document.tables.size(), 1U);
    const RichTextDocument::Table& table = document.tables[0];
    EXPECT_EQ(table.columns, 2U);
    ASSERT_EQ(table.cells.size(), 3U);
    EXPECT_EQ(table.cells[0].background, math::Color::fromHex(0x202020FFU));
    EXPECT_FLOAT_EQ(table.cells[0].padding, 6.0F);
    EXPECT_EQ(table.cells[1].paragraphs.size(), 2U);
    EXPECT_EQ(document.paragraphs[1].kind, RichTextDocument::Paragraph::Kind::Table);

    const RichTextDocument::Paragraph& story = document.paragraphs[2];
    ASSERT_TRUE(story.dropCap);
    EXPECT_EQ(story.dropCap->text, U"O");
    EXPECT_FLOAT_EQ(story.dropCap->margin, 8.0F);
    EXPECT_EQ(document.styles[story.dropCap->style].size, 64.0F);
    EXPECT_EQ(story.inlines[0].text, U"nce upon a time");
}

TEST_F(RichTextMarkupTest, ReportsMalformedMarkupWithItsPlace) {
    EXPECT_EQ(errorOf("ok [/b]"), "Rich text markup at line 1, column 4: The tag \"[/b]\" closes nothing.");
    EXPECT_EQ(errorOf("[b]\n [i]x[/b]"), "Rich text markup at line 2, column 6: The tag \"[/b]\" closes \"[i]\", which is still open.");
    EXPECT_EQ(errorOf("é[b]never"), "Rich text markup at line 1, column 2: The tag \"[b]\" is never closed.");
    EXPECT_EQ(errorOf("a [b"), "Rich text markup at line 1, column 3: A tag has no closing bracket.");
    EXPECT_EQ(errorOf("[color=blurple]x[/color]"), "Rich text markup at line 1, column 1: The value \"blurple\" is not a color. Use a name such as \"red\" or \"#RRGGBB\" or \"#AARRGGBB\".");
    EXPECT_EQ(errorOf("[size=big]x[/size]"), "Rich text markup at line 1, column 1: The value \"big\" is not a number for the size of \"[size]\".");
    EXPECT_EQ(errorOf("[b=1]x[/b]"), "Rich text markup at line 1, column 1: The tag \"[b]\" takes no value.");
    EXPECT_EQ(errorOf("[color]x[/color]"), "Rich text markup at line 1, column 1: The tag \"[color]\" needs a value, as in \"[color=...]\".");
    EXPECT_EQ(errorOf("[img=a.png spin=2]"), "Rich text markup at line 1, column 1: The tag \"[img]\" has no attribute named \"spin\".");
    EXPECT_EQ(errorOf("[ol type=x]a[/ol]"), "Rich text markup at line 1, column 1: The type of \"[ol]\" must be \"1\", \"a\", \"A\", \"i\" or \"I\".");
    EXPECT_EQ(errorOf("[table=2]text[/table]"), "Rich text markup at line 1, column 10: Text inside a \"[table]\" must be inside a \"[cell]\".");
    EXPECT_EQ(errorOf("[cell]x[/cell]"), "Rich text markup at line 1, column 1: A \"[cell]\" goes directly inside a \"[table]\".");
    EXPECT_EQ(errorOf("[table=1][cell][table=1][/table][/cell][/table]"), "Rich text markup at line 1, column 16: Tables cannot be nested.");
    EXPECT_EQ(errorOf("x[dropcap]A[/dropcap]"), "Rich text markup at line 1, column 2: A \"[dropcap]\" must start its paragraph.");
    EXPECT_EQ(errorOf("[wave amp]x[/wave]"), "Rich text markup at line 1, column 1: The attribute \"amp\" of \"[wave]\" needs a value.");
    EXPECT_EQ(errorOf("[hr width=5]"), "Rich text markup at line 1, column 1: The width of \"[hr]\" is a percentage such as \"50%\".");
    EXPECT_EQ(errorOf("[pause=-1]"), "Rich text markup at line 1, column 1: A \"[pause]\" cannot last a negative time.");
    EXPECT_EQ(errorOf("[]"), "Rich text markup at line 1, column 1: The markup \"[]\" has no tag name.");
}

} // namespace haylen::text
