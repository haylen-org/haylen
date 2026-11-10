#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/RichText.hpp"
#include "haylen/text/SystemFonts.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"
#include "support/FontCollection.hpp"
#include "text/FontFile.hpp"
#include "text/GlyphTables.hpp"

namespace haylen::text {

// An engine on a host whose only font of the system draws Japanese from the test font.
class SystemFontTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.host().addSystemFont(U"日本語", std::string(HAYLEN_TEST_FONTS) + "/mplus-1p-regular.ttf");
    }

    [[nodiscard]] SystemFonts& systemFonts() {
        return *fixture.engine().getPlugin<plugins::TextPlugin>().getSystemFonts();
    }

    [[nodiscard]] std::shared_ptr<FontFamily> defaultFamily() {
        return fixture.engine().getPlugin<plugins::TextPlugin>().getRegistry()->getDefaultFamily();
    }

    // Runs frames until every lookup of the fonts of the system answered.
    void settle() {
        fixture.frames(1);
        ASSERT_TRUE(fixture.frameUntil([this] { return !systemFonts().isWaiting(); }));
    }

    test::EngineFixture fixture;
};

// The family draws the missing glyph of its face while the font of the system loads off the frame thread, and lays the text out again with it once it arrives, having read its file once for the whole word.
TEST_F(SystemFontTest, DrawsWhatNoFontOfTheFamilyHoldsWithTheFontOfTheSystem) {
    const std::shared_ptr<FontFamily> family = defaultFamily();
    const Style style{.size = 32.0F};
    const std::shared_ptr<const Layout> waiting = family->layout("日本語", style);
    EXPECT_TRUE(waiting->waitingForFonts);
    EXPECT_EQ(waiting->looks[waiting->glyphs[0].look].font, fixture.engine().getDefaultFont().get());

    settle();
    ASSERT_EQ(systemFonts().getFaces().size(), 1U);
    EXPECT_EQ(fixture.host().getSystemFontReads(), 1);
    EXPECT_EQ(fixture.host().getSystemFontLookups(), 3);

    const std::shared_ptr<const Layout> drawn = family->layout("日本語", style);
    EXPECT_FALSE(drawn->waitingForFonts);
    ASSERT_EQ(drawn->glyphs.size(), 3U);
    for (const Layout::Glyph& glyph : drawn->glyphs) {
        EXPECT_EQ(drawn->looks[glyph.look].font, systemFonts().getFaces()[0].font.get());
    }
}

// A character that no font of the system draws is looked up once, and its text keeps the missing glyph of the face without waiting again.
TEST_F(SystemFontTest, LooksEveryCharacterUpOnce) {
    const std::shared_ptr<FontFamily> family = defaultFamily();
    const Style style{.size = 32.0F};
    EXPECT_TRUE(family->layout("한", style)->waitingForFonts);
    settle();
    EXPECT_FALSE(family->layout("한한", style)->waitingForFonts);
    fixture.frames(3);
    EXPECT_EQ(fixture.host().getSystemFontLookups(), 1);
    EXPECT_TRUE(systemFonts().getFaces().empty());
}

// A family without the fonts of the system never asks for them, and rich text lays itself out again once the font arrives.
TEST_F(SystemFontTest, LaysRichTextOutAgainAndLeavesFamiliesWithoutSystemFonts) {
    FontFamily alone(FontFamily::Faces{.regular = fixture.engine().getDefaultFont()});
    EXPECT_FALSE(alone.layout("日", {.size = 32.0F})->waitingForFonts);
    fixture.frames(3);
    EXPECT_EQ(fixture.host().getSystemFontLookups(), 0);

    RichText rich("[b]日本[/b]", {.family = defaultFamily(), .size = 32.0F}, fixture.engine().getPlugin<plugins::TextPlugin>().getRegistry());
    EXPECT_TRUE(rich.getLayout().waitingForFonts);
    settle();
    rich.update(0.0F);
    const Layout& laid = rich.getLayout();
    EXPECT_FALSE(laid.waitingForFonts);
    EXPECT_EQ(laid.looks[laid.glyphs[0].look].font, systemFonts().getFaces()[0].font.get());
}

// The faces of the system often come in collections, so a font reads the face that its options name and refuses one the collection lacks.
TEST_F(SystemFontTest, ReadsTheFaceOfACollection) {
    const std::vector<std::vector<std::uint8_t>> faces{test::FontCollection::read("crimson-text-regular.ttf"), test::FontCollection::read("mplus-1p-regular.ttf")};
    const std::vector<std::uint8_t> collection = test::FontCollection::build(faces);
    graphics::Device& device = fixture.engine().getGraphics();
    EXPECT_FALSE(TrueTypeFont(device, collection).hasGlyph(U'日'));
    EXPECT_TRUE(TrueTypeFont(device, collection, {.face = 1}).hasGlyph(U'日'));
    EXPECT_THROW(TrueTypeFont(device, collection, {.face = 2}), std::runtime_error);
    EXPECT_THROW(TrueTypeFont(device, collection, {.face = -1}), std::invalid_argument);

    const std::vector<std::uint8_t> cut(collection.begin(), collection.begin() + 18);
    EXPECT_THROW(TrueTypeFont(device, cut, {.face = 1}), std::runtime_error);
}

// The engine draws a face through its color layers, or through its outlines unless bitmaps hold its real glyphs, which HarfBuzz reads from the tables of the face, in a collection too.
TEST(GlyphTablesTest, DrawsColorLayersAndOutlinesWithoutBitmaps) {
    EXPECT_TRUE(GlyphTables::isDrawable(std::array<std::uint32_t, 2>{0x636D6170U, 0x676C7966U}));
    EXPECT_TRUE(GlyphTables::isDrawable(std::array<std::uint32_t, 2>{0x43464620U, 0x636D6170U}));
    EXPECT_TRUE(GlyphTables::isDrawable(std::array<std::uint32_t, 3>{0x43424454U, 0x434F4C52U, 0x676C7966U}));
    EXPECT_FALSE(GlyphTables::isDrawable(std::array<std::uint32_t, 2>{0x43424454U, 0x676C7966U}));
    EXPECT_FALSE(GlyphTables::isDrawable(std::array<std::uint32_t, 2>{0x676C7966U, 0x73626978U}));
    EXPECT_FALSE(GlyphTables::isDrawable(std::array<std::uint32_t, 1>{0x636D6170U}));

    const std::vector<std::uint8_t> letters = test::FontCollection::read("crimson-text-regular.ttf");
    const std::vector<std::uint8_t> bitmaps = test::FontCollection::addTable(letters, 0x43424454U);
    const std::vector<std::vector<std::uint8_t>> faces{letters, bitmaps};
    const std::vector<std::uint8_t> collection = test::FontCollection::build(faces);
    EXPECT_EQ(FontFile::countFaces(collection), 2U);
    EXPECT_TRUE(FontFile::read(std::span<const std::uint8_t>(collection), 0)->isDrawable());
    EXPECT_FALSE(FontFile::read(std::span<const std::uint8_t>(collection), 1)->isDrawable());
    EXPECT_FALSE(FontFile::read(std::span<const std::uint8_t>(collection), 2).has_value());
}

// A face whose glyphs are bitmaps, or that lacks the character the platform named it for, is refused and never read again.
TEST_F(SystemFontTest, RefusesFacesTheEngineCannotDrawAndReadsThemOnce) {
    const std::filesystem::path folder = std::filesystem::temp_directory_path() / ("haylen-system-fonts-" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
    std::filesystem::create_directories(folder);
    const std::vector<std::uint8_t> bitmaps = test::FontCollection::addTable(test::FontCollection::read("mplus-1p-regular.ttf"), 0x43424454U);
    std::ofstream(folder / "bitmaps.ttf", std::ios::binary).write(reinterpret_cast<const char*>(bitmaps.data()), static_cast<std::streamsize>(bitmaps.size()));
    fixture.host().addSystemFont(U"한글", folder / "bitmaps.ttf");
    fixture.host().addSystemFont(U"ก", std::string(HAYLEN_TEST_FONTS) + "/crimson-text-regular.ttf");

    const std::shared_ptr<FontFamily> family = defaultFamily();
    EXPECT_TRUE(family->layout("한ก", {.size = 32.0F})->waitingForFonts);
    settle();
    EXPECT_TRUE(family->layout("글", {.size = 32.0F})->waitingForFonts);
    settle();
    EXPECT_TRUE(systemFonts().getFaces().empty());
    EXPECT_EQ(fixture.host().getSystemFontReads(), 2);
    EXPECT_EQ(fixture.host().getSystemFontLookups(), 3);
    std::filesystem::remove_all(folder);
}

// Lua loads the face of a collection that the option names.
TEST(FontCollectionLuaTest, LoadsTheFaceOfACollection) {
    const std::vector<std::vector<std::uint8_t>> faces{test::FontCollection::read("crimson-text-regular.ttf"), test::FontCollection::read("mplus-1p-regular.ttf")};
    const std::vector<std::uint8_t> collection = test::FontCollection::build(faces);
    test::EngineFixture packaged({{"content/fonts/faces.ttc", std::string(collection.begin(), collection.end())}});
    packaged.runLua("assets = require('haylen.assets')");
    EXPECT_EQ(packaged.lua("return tostring(assets.font('fonts/faces.ttc', {face = 1}):hasGlyph('\u65E5')) .. ' ' .. tostring(assets.font('fonts/faces.ttc'):hasGlyph('\u65E5'))"), "true false");
    EXPECT_NE(packaged.lua("assets.font('fonts/faces.ttc', {face = 5})").find("no face 5"), std::string::npos);
}

// Lua names the faces that arrived, tells which families use the fonts of the system and makes families without them.
TEST_F(SystemFontTest, ReachesTheFontsOfTheSystemFromLua) {
    fixture.runLua(R"(graphics = require('haylen.graphics') graphics2d = require('haylen.graphics2d') family = graphics.newFontFamily({regular = graphics2d.defaultFont()}) alone = graphics.newFontFamily({regular = graphics2d.defaultFont(), systemFonts = false}))");
    EXPECT_EQ(fixture.lua("return tostring(family.systemFonts) .. ' ' .. tostring(alone.systemFonts) .. ' ' .. #graphics.systemFonts()"), "true false 0");
    EXPECT_EQ(fixture.lua("local font, bold, italic, waiting = family:resolve('日') return tostring(font == graphics2d.defaultFont()) .. ' ' .. tostring(waiting)"), "true true");
    settle();
    EXPECT_EQ(fixture.lua("local font, bold, italic, waiting = family:resolve('日') return tostring(font:hasGlyph('日')) .. ' ' .. tostring(waiting) .. ' ' .. #graphics.systemFonts()"), "true false 1");
    EXPECT_NE(fixture.lua("return graphics.systemFonts()[1]").find("mplus-1p-regular.ttf#0"), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newFontFamily({regular = graphics2d.defaultFont(), systemFonts = 'yes'})").find("systemFonts"), std::string::npos);
}

} // namespace haylen::text
