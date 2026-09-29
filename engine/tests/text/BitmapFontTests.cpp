#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/text/BitmapFont.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::text {

namespace {

// A BMFont with two pages, a quoted face name holding spaces, and a kerning pair.
constexpr const char* kTextFont = "info face=\"Pixel Sans\" size=-16 bold=0 italic=0 padding=0,0,0,0 spacing=1,1\r\n"
                                  "common lineHeight=18 base=14 scaleW=64 scaleH=64 pages=2 packed=0\n"
                                  "page id=0 file=\"pixel_0.png\"\n"
                                  "page id=1 file=\"pixel_1.png\"\n"
                                  "chars count=3\n"
                                  "char id=65 x=0 y=0 width=8 height=12 xoffset=1 yoffset=2 xadvance=10 page=0 chnl=15\n"
                                  "char id=66 x=8 y=0 width=8 height=12 xoffset=0 yoffset=2 xadvance=9 page=1 chnl=15\n"
                                  "char id=32 x=0 y=0 width=0 height=0 xoffset=0 yoffset=0 xadvance=5 page=0 chnl=15\n"
                                  "kernings count=1\n"
                                  "kerning first=65 second=66 amount=-2\n";

void writeNumber(std::vector<std::uint8_t>& bytes, std::uint32_t value, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index) {
        bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8U)));
    }
}

// The same font in the binary format: the magic, version 3 and its info, common, pages, chars and kerning blocks.
std::vector<std::uint8_t> binaryFont() {
    std::vector<std::uint8_t> bytes{'B', 'M', 'F', 3};
    const std::string name = "Pixel Sans";
    bytes.push_back(1);
    writeNumber(bytes, static_cast<std::uint32_t>(14 + name.size() + 1), 4);
    writeNumber(bytes, static_cast<std::uint16_t>(-16), 2);
    bytes.insert(bytes.end(), 12, 0);
    bytes.insert(bytes.end(), name.begin(), name.end());
    bytes.push_back(0);

    bytes.push_back(2);
    writeNumber(bytes, 15, 4);
    for (const std::uint32_t value : {18U, 14U, 64U, 64U, 2U}) {
        writeNumber(bytes, value, 2);
    }
    bytes.insert(bytes.end(), 5, 0);

    const std::string pages = std::string("pixel_0.png") + '\0' + "pixel_1.png" + '\0';
    bytes.push_back(3);
    writeNumber(bytes, static_cast<std::uint32_t>(pages.size()), 4);
    bytes.insert(bytes.end(), pages.begin(), pages.end());

    bytes.push_back(4);
    writeNumber(bytes, 60, 4);
    for (const auto& [id, x, xoffset, advance, page, width] : std::vector<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>>{{65, 0, 1, 10, 0, 8}, {66, 8, 0, 9, 1, 8}, {32, 0, 0, 5, 0, 0}}) {
        writeNumber(bytes, id, 4);
        writeNumber(bytes, x, 2);
        writeNumber(bytes, 0, 2);
        writeNumber(bytes, width, 2);
        writeNumber(bytes, width == 0 ? 0 : 12, 2);
        writeNumber(bytes, xoffset, 2);
        writeNumber(bytes, id == 32 ? 0 : 2, 2);
        writeNumber(bytes, advance, 2);
        bytes.push_back(static_cast<std::uint8_t>(page));
        bytes.push_back(15);
    }

    bytes.push_back(5);
    writeNumber(bytes, 10, 4);
    writeNumber(bytes, 65, 4);
    writeNumber(bytes, 66, 4);
    writeNumber(bytes, static_cast<std::uint16_t>(-2), 2);
    return bytes;
}

void expectPixelFont(const BitmapFont::Description& description) {
    EXPECT_FLOAT_EQ(description.size, 16.0F);
    EXPECT_FLOAT_EQ(description.lineHeight, 18.0F);
    EXPECT_FLOAT_EQ(description.base, 14.0F);
    ASSERT_EQ(description.pages, (std::vector<std::string>{"pixel_0.png", "pixel_1.png"}));
    ASSERT_EQ(description.characters.size(), 3U);
    EXPECT_EQ(description.characters[0].codePoint, U'A');
    EXPECT_EQ(description.characters[0].source, math::Rect(0.0F, 0.0F, 8.0F, 12.0F));
    EXPECT_EQ(description.characters[0].offset, math::Vec2(1.0F, -12.0F));
    EXPECT_FLOAT_EQ(description.characters[0].advance, 10.0F);
    EXPECT_EQ(description.characters[1].page, 1U);
    ASSERT_EQ(description.kernings.size(), 1U);
    EXPECT_FLOAT_EQ(description.kernings[0].amount, -2.0F);
}

std::vector<std::uint8_t> bytesOf(const std::string& text) {
    return {text.begin(), text.end()};
}

} // namespace

TEST(BitmapFontTest, ReadsTextAndBinaryBmFonts) {
    expectPixelFont(BitmapFont::parse(bytesOf(kTextFont)));
    expectPixelFont(BitmapFont::parse(binaryFont()));

    EXPECT_THROW((void)BitmapFont::parse(bytesOf("common lineHeight=18 base=14 pages=1\npage id=0 file=\"a.png\"\n")), std::invalid_argument);
    EXPECT_THROW((void)BitmapFont::parse(bytesOf("info size=16\ncommon lineHeight=18 base=14 pages=1\n")), std::invalid_argument);
    EXPECT_THROW((void)BitmapFont::parse(bytesOf("info size=16\ncommon lineHeight=18 base=14 pages=1\npage id=3 file=\"a.png\"\n")), std::invalid_argument);
    EXPECT_THROW((void)BitmapFont::parse(bytesOf("info size=16\ncommon lineHeight=x base=14 pages=1\n")), std::invalid_argument);
    std::vector<std::uint8_t> truncated = binaryFont();
    truncated.resize(truncated.size() - 4);
    EXPECT_THROW((void)BitmapFont::parse(truncated), std::invalid_argument);
    EXPECT_THROW((void)BitmapFont::parse(std::vector<std::uint8_t>{'B', 'M', 'F', 2}), std::invalid_argument);
    EXPECT_THROW((void)BitmapFont::parse(std::vector<std::uint8_t>{'B', 'M', 'F', 3, 9, 0, 0, 0, 0}), std::invalid_argument);
}

TEST(BitmapFontTest, DescribesGridsOfCells) {
    const BitmapFont::Description grid = BitmapFont::describeGrid({.characters = "ABCDE", .cellWidth = 8.0F, .cellHeight = 10.0F, .spacing = {2.0F, 1.0F}, .margin = {1.0F, 3.0F}, .advance = 7.0F, .baseline = 8.0F}, {33.0F, 30.0F});
    ASSERT_EQ(grid.characters.size(), 5U);
    EXPECT_EQ(grid.characters[0].source, math::Rect(1.0F, 3.0F, 8.0F, 10.0F));
    EXPECT_EQ(grid.characters[2].source, math::Rect(21.0F, 3.0F, 8.0F, 10.0F));
    EXPECT_EQ(grid.characters[3].source, math::Rect(1.0F, 14.0F, 8.0F, 10.0F));
    EXPECT_EQ(grid.characters[0].offset, math::Vec2(0.0F, -8.0F));
    EXPECT_FLOAT_EQ(grid.characters[4].advance, 7.0F);
    EXPECT_FLOAT_EQ(grid.lineHeight, 10.0F);

    EXPECT_THROW((void)BitmapFont::describeGrid({.characters = "ABCDEFG", .cellWidth = 8.0F, .cellHeight = 8.0F}, {16.0F, 16.0F}), std::invalid_argument);
    EXPECT_THROW((void)BitmapFont::describeGrid({.characters = "", .cellWidth = 8.0F, .cellHeight = 8.0F}, {16.0F, 16.0F}), std::invalid_argument);
    EXPECT_THROW((void)BitmapFont::describeGrid({.characters = "A", .cellWidth = 0.0F, .cellHeight = 8.0F}, {16.0F, 16.0F}), std::invalid_argument);
}

TEST(BitmapFontTest, LoadsAsAssetsAndDrawsLikeAnyFont) {
    const std::vector<std::uint8_t> page = test::pngImage(64, 64, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/fonts/pixel.fnt", kTextFont}, {"content/fonts/pixel_0.png", std::string(page.begin(), page.end())}, {"content/fonts/pixel_1.png", std::string(page.begin(), page.end())}, {"content/fonts/grid.png", std::string(page.begin(), page.end())}});
    core::Engine& engine = fixture.engine();
    assets::Manager& assets = engine.getAssets();

    const auto font = std::static_pointer_cast<Font>(assets.load(assets.getTypeForPath("fonts/pixel.fnt"), "fonts/pixel.fnt"));
    EXPECT_FALSE(font->isDistanceField());
    EXPECT_EQ(font->getPageCount(), 2U);
    EXPECT_FLOAT_EQ(font->getNativeSize(), 16.0F);
    EXPECT_FLOAT_EQ(font->getLineHeight(32.0F), 36.0F);
    EXPECT_FLOAT_EQ(font->getAscent(16.0F), 14.0F);
    EXPECT_TRUE(font->hasGlyph(U'A'));
    EXPECT_FALSE(font->hasGlyph(U'Z'));
    EXPECT_FALSE(font->getGlyph(U'Z').visible);
    EXPECT_FLOAT_EQ(font->toDistance(4.0F, 16.0F), 0.0F);
    EXPECT_EQ(font->getPage(0), assets.texture("fonts/pixel_0.png"));
    EXPECT_THROW((void)font->getPage(2), std::out_of_range);

    // Kerning pulls B two pixels closer, and B comes from the second page. A right-to-left run reads backwards with its pairs in screen order.
    const std::shared_ptr<const TextLayout> laid = font->layout("AB Z", {.size = 16.0F});
    ASSERT_EQ(laid->glyphs.size(), 2U);
    EXPECT_FLOAT_EQ(laid->glyphs[0].position.x, 1.0F);
    EXPECT_FLOAT_EQ(laid->glyphs[0].position.y, 2.0F);
    EXPECT_FLOAT_EQ(laid->glyphs[1].position.x, 8.0F);
    EXPECT_EQ(laid->glyphs[1].page, 1U);
    EXPECT_FLOAT_EQ(font->measure("AB", {.size = 32.0F}).x, 34.0F);
    std::vector<Font::ShapedGlyph> shaped;
    const std::u32string text = U"AB";
    font->shape({.text = text, .begin = 0, .end = 2, .rightToLeft = true}, shaped);
    ASSERT_EQ(shaped.size(), 2U);
    EXPECT_EQ(shaped[0].index, static_cast<std::uint32_t>(U'B'));
    EXPECT_EQ(shaped[0].cluster, 1U);
    EXPECT_FLOAT_EQ(shaped[0].advance, font->getGlyph(U'B').advance);

    const auto grid = std::static_pointer_cast<Font>(assets.load("gridFont", "fonts/grid.png", {{"characters", "0123456789"}, {"cellWidth", 8}, {"cellHeight", 8}, {"filter", "nearest"}}));
    EXPECT_TRUE(grid->hasGlyph(U'9'));
    EXPECT_EQ(assets.load("gridFont", "fonts/grid.png", {{"characters", "0123456789"}, {"cellWidth", 8}, {"cellHeight", 8}}), assets.load("gridFont", "fonts/grid.png", {{"characters", "0123456789"}, {"cellWidth", 8.0}, {"cellHeight", 8.0}, {"spacing", {0, 0}}}));
    EXPECT_THROW((void)assets.load("gridFont", "fonts/grid.png", {{"cellWidth", 8}, {"cellHeight", 8}}), std::invalid_argument);
    EXPECT_THROW((void)assets.load("gridFont", "fonts/grid.png", {{"characters", "0"}, {"cellWidth", 8}, {"cellHeight", 8}, {"spacing", 2}}), std::invalid_argument);

    fixture.frames(1);
    // clang-format off
    engine.getScenes().push(std::make_shared<test::DrawingScene>([&](core::Engine& current) {
        graphics2d::Renderer& renderer = current.getRenderer2D();
        renderer.beginScreen();
        renderer.drawText(*font, "AB", {10.0F, 10.0F}, {.size = 32.0F, .shadowOffset = {1.0F, 1.0F}, .shadowColor = math::Color::black()});
        renderer.drawText(*grid, "42", {10.0F, 40.0F});
    }));
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(engine.getRenderer2D().getStats().sprites, 6U);
    EXPECT_THROW(BitmapFont(BitmapFont::parse(bytesOf(kTextFont)), {}), std::invalid_argument);
}

} // namespace haylen::text
