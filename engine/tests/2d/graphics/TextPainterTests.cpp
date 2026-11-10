#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "2d/graphics/GpuInstance.hpp"
#include "2d/graphics/TextPainter.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/text/BitmapFont.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/Gradient.hpp"
#include "haylen/text/RichText.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::graphics2d {

namespace {

class TextPainterTest : public ::testing::Test {
  protected:
    [[nodiscard]] text::RichText make(const std::string& markup, text::RichTextOptions options = {}) {
        const std::shared_ptr<text::RichTextRegistry> registry = fixture.engine().getPlugin<plugins::TextPlugin>().getRegistry();
        if (!options.family) {
            options.family = registry->getDefaultFamily();
        }
        return {markup, std::move(options), registry};
    }

    [[nodiscard]] std::vector<TextPainter::Batch> paint(text::RichText& richText) {
        TextPainter painter = makePainter();
        painter.paintRichText(richText.getFrame(), {100.0F, 50.0F}, {});
        const std::span<const TextPainter::Batch> batches = painter.getBatches();
        return {batches.begin(), batches.end()};
    }

    // The default font with the color emoji test font as its fallback.
    [[nodiscard]] std::shared_ptr<text::FontFamily> makeEmojiFamily() {
        std::ifstream file(std::string(HAYLEN_TEST_FONTS) + "/noto-color-emoji-regular.ttf", std::ios::binary);
        auto emoji = std::make_shared<text::TrueTypeFont>(fixture.engine().getGraphics(), std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()));
        return std::make_shared<text::FontFamily>(text::FontFamily::Faces{.regular = fixture.engine().getDefaultFont(), .fallbacks = {emoji}});
    }

    [[nodiscard]] TextPainter makePainter() {
        return {fixture.engine().getGraphics().getWhiteTexture(), atlas};
    }

    // Reads a field of the record of a gradient at the offset the vertex layout of the gradient text program gives it.
    template <typename T> [[nodiscard]] static T at(const GpuInstance& record, std::size_t offset) {
        const auto bytes = std::bit_cast<std::array<std::uint8_t, sizeof(GpuInstance)>>(record);
        T value{};
        std::memcpy(&value, bytes.data() + offset, sizeof(T));
        return value;
    }

    // The position along the gradient of a record at a point of the canvas, as the vertex stage takes it.
    [[nodiscard]] static float positionAlong(const GpuInstance& record, math::Vec2 point) {
        return point.x * at<float>(record, 0) + point.y * at<float>(record, 4) + at<float>(record, 8);
    }

    // Decodes a parameter byte the way the shaders read a signed one.
    [[nodiscard]] static int signedByte(std::uint8_t value) {
        return value < 128 ? value : value - 256;
    }

    test::EngineFixture fixture;
    VectorAtlas atlas{fixture.engine().getGraphics(), fixture.engine().getJobs()};
};

} // namespace

// A synthetic bold spreads the glyph by a share of its quad along the baseline instead of moving the edge of its field, so the gaps above and below its strokes stay as the font draws them.
TEST_F(TextPainterTest, PacksSyntheticStylesIntoTheGlyphParameters) {
    text::RichText styled = make("[b]b[/b][i]i[/i][outline=2 color=red]o[/outline]", {.size = 32.0F});
    const std::vector<TextPainter::Batch> batches = paint(styled);
    ASSERT_EQ(batches.size(), 1U);
    EXPECT_EQ(batches[0].program, Program::Text);
    ASSERT_EQ(batches[0].instances.size(), 4U);

    const GpuInstance& bold = batches[0].instances[1];
    const text::Font& font = *styled.getLayout().looks[0].font;
    const text::Layout::Glyph& widened = styled.getLayout().glyphs[0];
    EXPECT_NEAR(static_cast<float>(bold.parameters[1]) / 255.0F * 0.25F, 32.0F * 0.03F / widened.size.x, 0.25F / 255.0F);
    EXPECT_EQ(bold.parameters[2], 0U);
    EXPECT_EQ(bold.parameters[0], 0U);

    // Italics lean around the baseline, where their pivot sits.
    const GpuInstance& italic = batches[0].instances[2];
    EXPECT_EQ(signedByte(italic.parameters[2]), static_cast<int>(std::lround(0.2F * 127.0F)));
    EXPECT_EQ(italic.parameters[1], 0U);
    const text::Layout::Glyph& leaning = styled.getLayout().glyphs[1];
    EXPECT_FLOAT_EQ(italic.position[1], 50.0F + leaning.baseline);
    EXPECT_NEAR(italic.position[1] - italic.pivot[1] * italic.size[1], 50.0F + leaning.position.y, 0.001F);

    // The outline is a glyph of its own under every fill, and the fill over it has none.
    const GpuInstance& outline = batches[0].instances[0];
    const GpuInstance& outlined = batches[0].instances[3];
    EXPECT_NEAR(static_cast<float>(outline.parameters[0]) / 255.0F * 0.5F, font.toDistance(2.0F, 32.0F), 0.5F / 255.0F);
    EXPECT_EQ(outline.flash, math::Color::fromHex(0xFF0000FFU).toRgba8());
    EXPECT_EQ(outline.color >> 24U, 0U);
    EXPECT_EQ(outlined.parameters[0], 0U);
    EXPECT_EQ(outlined.flash, 0U);
    EXPECT_FLOAT_EQ(outline.position[0], outlined.position[0]);
}

// An outline that reaches past the side of its glyph would cover the letter next to it if it drew with its own fill, so every outline draws first.
TEST_F(TextPainterTest, DrawsEveryOutlineUnderEveryFill) {
    text::Font& font = *fixture.engine().getDefaultFont();
    const text::Style style{.size = 48.0F, .color = math::Color::white(), .outlineWidth = 4.0F, .outlineColor = math::Color::black()};
    TextPainter painter = makePainter();
    painter.paintText(*font.layout("WAVE", style), {}, style, {});
    ASSERT_EQ(painter.getBatches().size(), 1U);
    const std::vector<GpuInstance>& instances = painter.getBatches()[0].instances;
    ASSERT_EQ(instances.size(), 8U);
    for (std::size_t index = 0; index < 4; ++index) {
        const GpuInstance& outline = instances[index];
        const GpuInstance& fill = instances[index + 4];
        EXPECT_GT(outline.parameters[0], 0U);
        EXPECT_EQ(outline.color >> 24U, 0U);
        EXPECT_EQ(outline.flash, math::Color::black().toRgba8());
        EXPECT_EQ(fill.parameters[0], 0U);
        EXPECT_EQ(fill.color, math::Color::white().toRgba8());
        EXPECT_EQ(fill.flash, 0U);
        EXPECT_FLOAT_EQ(outline.position[0], fill.position[0]);
    }

    text::RichText rich = make("[outline=4 color=black]WAVE[/outline]", {.size = 48.0F});
    const std::vector<TextPainter::Batch> batches = paint(rich);
    ASSERT_EQ(batches.size(), 1U);
    ASSERT_EQ(batches[0].instances.size(), 8U);
    for (std::size_t index = 0; index < 4; ++index) {
        EXPECT_GT(batches[0].instances[index].parameters[0], 0U);
        EXPECT_EQ(batches[0].instances[index + 4].parameters[0], 0U);
    }
}

TEST_F(TextPainterTest, ScalesAndTintsRichTextFromItsCorner) {
    text::RichText boxed = make("[bgcolor=white]ab[/bgcolor]", {.size = 20.0F});
    TextPainter painter = makePainter();
    painter.paintRichText(boxed.getLayout(), {10.0F, 20.0F}, {}, {2.0F, 3.0F}, math::Color{1.0F, 1.0F, 1.0F, 0.5F});
    ASSERT_EQ(painter.getBatches().size(), 2U);
    const GpuInstance& background = painter.getBatches()[0].instances[0];
    const math::Rect& box = boxed.getLayout().boxes[0].rect;
    EXPECT_FLOAT_EQ(background.position[0], 10.0F + box.x * 2.0F);
    EXPECT_FLOAT_EQ(background.size[1], box.height * 3.0F);
    EXPECT_NEAR(static_cast<double>(background.color >> 24U), 127.5, 1.0);

    const GpuInstance& glyph = painter.getBatches()[1].instances[0];
    const text::Layout::Glyph& laid = boxed.getLayout().glyphs[0];
    EXPECT_FLOAT_EQ(glyph.position[1], 20.0F + laid.baseline * 3.0F);
    EXPECT_FLOAT_EQ(glyph.size[0], laid.size.x * 2.0F);
}

TEST_F(TextPainterTest, DrawsShadowsAndGlowsUnderTheGlyphs) {
    text::RichText lit = make("[bgcolor=#203040][glow=6 color=yellow][shadow=3,3 blur=2]g[/shadow][/glow][/bgcolor][u]u[/u]", {.size = 32.0F});
    const std::vector<TextPainter::Batch> batches = paint(lit);
    ASSERT_EQ(batches.size(), 3U);
    EXPECT_EQ(batches[0].program, Program::Sprite);
    EXPECT_EQ(batches[0].instances.size(), 1U);
    EXPECT_EQ(batches[0].instances[0].color, math::Color::fromHex(0x203040FFU).toRgba8());
    EXPECT_EQ(batches[1].program, Program::Text);
    ASSERT_EQ(batches[1].instances.size(), 4U);
    EXPECT_EQ(batches[2].program, Program::Sprite);

    const GpuInstance& glow = batches[1].instances[0];
    const GpuInstance& shadow = batches[1].instances[1];
    const GpuInstance& glyph = batches[1].instances[2];
    EXPECT_GT(glow.parameters[0], 0U);
    EXPECT_EQ(glow.parameters[1], 0U);
    EXPECT_GT(glow.parameters[3], 0U);
    EXPECT_EQ(glow.color >> 24U, 0U);
    EXPECT_EQ(glow.flash, math::Color::fromHex(0xFFFF00FFU).toRgba8());
    EXPECT_GT(shadow.parameters[3], 0U);
    EXPECT_FLOAT_EQ(shadow.position[0], glyph.position[0] + 3.0F);
    EXPECT_EQ(glyph.parameters[3], 0U);
}

TEST_F(TextPainterTest, DrawsBitmapGlyphsAsSprites) {
    const graphics::Texture page = fixture.engine().getGraphics().createTexture(graphics::Image(16, 8, math::Color::white()));
    const std::shared_ptr<text::Font> grid = std::make_shared<text::BitmapFont>(text::BitmapFont::describeGrid({.characters = "AB", .cellWidth = 8.0F, .cellHeight = 8.0F}, page.getSize()), std::vector<graphics::Texture>{page});
    text::RichText pixel = make("A[b]B[/b][i]A[/i][shadow=1,1]B[/shadow]", {.family = std::make_shared<text::FontFamily>(text::FontFamily::Faces{.regular = grid}), .size = 16.0F});
    const std::vector<TextPainter::Batch> batches = paint(pixel);
    ASSERT_EQ(batches.size(), 1U);
    EXPECT_EQ(batches[0].program, Program::Sprite);
    EXPECT_EQ(batches[0].texture, page);

    // The shadow of the last glyph draws first, then every glyph, and the bold one twice.
    const std::vector<GpuInstance>& instances = batches[0].instances;
    ASSERT_EQ(instances.size(), 6U);
    EXPECT_EQ(instances[0].flash >> 24U, 255U);
    EXPECT_FLOAT_EQ(instances[3].position[0], instances[2].position[0] + 2.0F);
    EXPECT_EQ(signedByte(instances[4].parameters[2]), static_cast<int>(std::lround(0.2F * 127.0F)));
    EXPECT_EQ(instances[1].parameters[0], 0U);

    TextPainter plain = makePainter();
    const text::Style style{.size = 16.0F, .shadowOffset = {1.0F, 1.0F}, .shadowColor = math::Color::black(), .shadowBlur = 4.0F};
    plain.paintText(*grid->layout("AB", style), {}, style, {});
    ASSERT_EQ(plain.getBatches().size(), 1U);
    EXPECT_EQ(plain.getBatches()[0].program, Program::Sprite);
    EXPECT_EQ(plain.getBatches()[0].instances[0].parameters[3], 0U);

    text::Font& field = *fixture.engine().getDefaultFont();
    TextPainter smooth = makePainter();
    smooth.paintText(*field.layout("AB", style), {}, style, {});
    ASSERT_EQ(smooth.getBatches().size(), 1U);
    EXPECT_EQ(smooth.getBatches()[0].program, Program::Text);
    EXPECT_GT(smooth.getBatches()[0].instances[0].parameters[3], 0U);
    EXPECT_EQ(smooth.getBatches()[0].instances[2].parameters[3], 0U);
}

// A color glyph draws the raster of its vector image through the sprite program in its own colors with the alpha of the text, its shadow is its silhouette, and it takes no outline.
TEST_F(TextPainterTest, DrawsColorGlyphsAsRastersInTheirOwnColors) {
    const std::shared_ptr<text::FontFamily> family = makeEmojiFamily();
    text::RichText mixed = make("A\U0001F600", {.family = family, .size = 32.0F, .color = math::Color{1.0F, 0.0F, 0.0F, 0.5F}});
    const std::vector<TextPainter::Batch> batches = paint(mixed);
    ASSERT_EQ(batches.size(), 2U);
    EXPECT_EQ(batches[0].program, Program::Text);
    EXPECT_EQ(batches[1].program, Program::Sprite);
    ASSERT_EQ(batches[1].instances.size(), 1U);
    const GpuInstance& emoji = batches[1].instances[0];
    EXPECT_EQ(emoji.color, math::Color(1.0F, 1.0F, 1.0F, 0.5F).toRgba8());
    EXPECT_EQ(emoji.flash, 0U);
    EXPECT_FLOAT_EQ(emoji.size[0], mixed.getLayout().glyphs[1].size.x);

    const text::Style style{.size = 32.0F, .outlineWidth = 2.0F, .outlineColor = math::Color::black(), .shadowOffset = {2.0F, 2.0F}, .shadowColor = math::Color{0.0F, 0.0F, 0.5F, 0.5F}};
    const std::shared_ptr<const text::Layout> face = family->layout("\U0001F602", style);
    TextPainter painter = makePainter();
    painter.paintText(*face, {}, style, {.pixelsPerUnit = 3.0F, .frame = 1});
    ASSERT_EQ(painter.getBatches().size(), 1U);
    const std::vector<GpuInstance>& instances = painter.getBatches()[0].instances;
    ASSERT_EQ(instances.size(), 2U);
    EXPECT_EQ(instances[0].flash, math::Color(0.0F, 0.0F, 0.5F, 1.0F).toRgba8());
    EXPECT_FLOAT_EQ(instances[0].position[0], instances[1].position[0] + 2.0F);
    EXPECT_EQ(instances[1].color, math::Color::white().toRgba8());

    // A canvas of three pixels a unit takes the raster of the step that covers three times the size of the glyph.
    const text::Layout::Glyph& glyph = face->glyphs[0];
    const float scale = glyph.size.x / glyph.image.getSize().x * 3.0F;
    EXPECT_EQ(atlas.find(glyph.image, scale, 1).source.width, std::ceil(glyph.image.getSize().x * VectorAtlas::scaleOf(VectorAtlas::stepOf(scale))));
    EXPECT_EQ(painter.getBatches()[0].texture, atlas.find(glyph.image, scale, 1).texture);
}

TEST_F(TextPainterTest, StretchesPlainTextByTheScaleOfItsStyle) {
    const graphics::Texture page = fixture.engine().getGraphics().createTexture(graphics::Image(16, 8, math::Color::white()));
    const std::shared_ptr<text::Font> grid = std::make_shared<text::BitmapFont>(text::BitmapFont::describeGrid({.characters = "AB", .cellWidth = 8.0F, .cellHeight = 8.0F}, page.getSize()), std::vector<graphics::Texture>{page});
    const text::Style style{.size = 8.0F};
    const text::Style stretched{.size = 8.0F, .scale = {2.0F, 3.0F}};
    TextPainter plain = makePainter();
    plain.paintText(*grid->layout("AB", style), {10.0F, 20.0F}, style, {});
    TextPainter wide = makePainter();
    wide.paintText(*grid->layout("AB", stretched), {10.0F, 20.0F}, stretched, {});

    // Every glyph grows by the scale and moves away from the position, where the anchor stays.
    const std::vector<GpuInstance>& before = plain.getBatches()[0].instances;
    const std::vector<GpuInstance>& after = wide.getBatches()[0].instances;
    ASSERT_EQ(after.size(), before.size());
    for (std::size_t index = 0; index < before.size(); ++index) {
        EXPECT_FLOAT_EQ(after[index].size[0], before[index].size[0] * 2.0F);
        EXPECT_FLOAT_EQ(after[index].size[1], before[index].size[1] * 3.0F);
        EXPECT_FLOAT_EQ(after[index].position[0] - 10.0F, (before[index].position[0] - 10.0F) * 2.0F);
        EXPECT_FLOAT_EQ(after[index].position[1] - 20.0F, (before[index].position[1] - 20.0F) * 3.0F);
    }
}

// A plain layout serves its text in every color, so its underline takes the color of each style that draws it.
TEST_F(TextPainterTest, DrawsTheLinesOfPlainTextInTheColorOfTheStyle) {
    text::Font& font = *fixture.engine().getDefaultFont();
    const graphics::Texture& white = fixture.engine().getGraphics().getWhiteTexture();
    for (const math::Color color : {math::Color{1.0F, 0.0F, 0.0F, 1.0F}, math::Color{0.0F, 0.0F, 1.0F, 1.0F}}) {
        const text::Style style{.size = 16.0F, .color = color, .underline = true};
        TextPainter painter = makePainter();
        painter.paintText(*font.layout("Line", style), {}, style, {});
        const TextPainter::Batch& line = painter.getBatches().back();
        ASSERT_EQ(line.texture, white);
        ASSERT_EQ(line.instances.size(), 1U);
        EXPECT_EQ(line.instances[0].color, GpuInstance::make(*white.getResource(), {.color = color}).color);
    }
}

// On a display with two pixels a unit, snapped text moves as one block to the nearest pixel column and puts every baseline on the nearest pixel row.
TEST_F(TextPainterTest, SnapsSmallTextToWholePixels) {
    text::Font& font = *fixture.engine().getDefaultFont();
    const text::Style style{.size = 11.0F, .lineSpacing = 1.13F, .pixelSnap = true};
    const std::shared_ptr<const text::Layout> layout = font.layout("Snap\nGrid", style);
    const TextPainter::PixelGrid grid{.scale = {2.0F, 2.0F}};
    TextPainter snapped = makePainter();
    snapped.paintText(*layout, {10.3F, 20.6F}, style, {}, grid);
    TextPainter loose = makePainter();
    loose.paintText(*layout, {10.3F, 20.6F}, style, {});

    const std::vector<GpuInstance>& after = snapped.getBatches()[0].instances;
    const std::vector<GpuInstance>& before = loose.getBatches()[0].instances;
    ASSERT_EQ(after.size(), layout->glyphs.size());
    for (std::size_t index = 0; index < after.size(); ++index) {
        EXPECT_NEAR(after[index].position[0] - before[index].position[0], 0.2F, 0.0001F);
        EXPECT_FLOAT_EQ(after[index].position[1] * 2.0F, std::round(before[index].position[1] * 2.0F));
    }

    // Every centered line starts on a pixel column of its own, though a line half a pixel narrower than the block would start half a pixel in.
    const text::Style centered{.size = 11.0F, .align = text::Alignment::Center, .pixelSnap = true};
    const std::shared_ptr<const text::Layout> lines = font.layout("Snap the grid\nGrid\ni", centered);
    TextPainter middle = makePainter();
    middle.paintText(*lines, {10.3F, 20.6F}, centered, {}, grid);
    TextPainter unsnapped = makePainter();
    unsnapped.paintText(*lines, {10.3F, 20.6F}, centered, {});
    std::size_t line = 0;
    for (std::size_t index = 0; index < lines->glyphs.size(); ++index) {
        while (lines->glyphs[index].character >= lines->lines[line].endCharacter) {
            ++line;
        }
        const float left = 10.3F + lines->lines[line].box.x;
        EXPECT_NEAR(middle.getBatches()[0].instances[index].position[0] - unsnapped.getBatches()[0].instances[index].position[0], std::round(left * 2.0F) / 2.0F - left, 0.0001F);
    }

    // Turned text keeps its place, since no pixel row runs along its baselines.
    const text::Style turned{.size = 11.0F, .rotation = 0.5F, .pixelSnap = true};
    TextPainter rotated = makePainter();
    rotated.paintText(*layout, {10.3F, 20.6F}, turned, {}, grid);
    TextPainter free = makePainter();
    free.paintText(*layout, {10.3F, 20.6F}, turned, {});
    EXPECT_FLOAT_EQ(rotated.getBatches()[0].instances[0].position[0], free.getBatches()[0].instances[0].position[0]);
    EXPECT_FLOAT_EQ(rotated.getBatches()[0].instances[0].position[1], free.getBatches()[0].instances[0].position[1]);
}

// A glyph with a gradient draws through the gradient text program with the record of its gradient after it, which runs from the top of its line to the bottom, so every line shows the whole gradient.
TEST_F(TextPainterTest, FillsGlyphsWithTheGradientOfTheirLine) {
    text::Font& font = *fixture.engine().getDefaultFont();
    const std::array<math::Color, 2> colors{math::Color::fromHex(0xFF0000FFU), math::Color::fromHex(0x0000FFFFU)};
    const text::Style style{.size = 32.0F, .color = math::Color{1.0F, 1.0F, 1.0F, 0.5F}, .gradient = text::Gradient::fromColors(colors)};
    const std::shared_ptr<const text::Layout> layout = font.layout("Ab\nCd", style);
    TextPainter painter = makePainter();
    painter.paintText(*layout, {100.0F, 50.0F}, style, {});
    ASSERT_EQ(painter.getBatches().size(), 1U);
    EXPECT_EQ(painter.getBatches()[0].program, Program::GradientText);
    const std::vector<GpuInstance>& instances = painter.getBatches()[0].instances;
    ASSERT_EQ(instances.size(), layout->glyphs.size() * 2);

    for (std::size_t index = 0; index < layout->glyphs.size(); ++index) {
        const GpuInstance& glyph = instances[index * 2];
        const GpuInstance& record = instances[index * 2 + 1];
        const text::Layout::Line& line = layout->lines[index < 2 ? 0 : 1];
        EXPECT_EQ(glyph.color, style.color.toRgba8());
        EXPECT_NEAR(positionAlong(record, {100.0F + line.box.x, 50.0F + line.box.y}), 0.0F, 0.0001F);
        EXPECT_NEAR(positionAlong(record, {100.0F + line.box.getRight(), 50.0F + line.box.getBottom()}), 1.0F, 0.0001F);
        EXPECT_EQ(at<std::uint32_t>(record, 12), colors[0].toRgba8());
        EXPECT_EQ(at<std::uint32_t>(record, 16), colors[1].toRgba8());
        EXPECT_EQ(at<std::uint32_t>(record, 24), colors[1].toRgba8());
        EXPECT_EQ(at<std::uint16_t>(record, 28), 0U);
        EXPECT_EQ(at<std::uint16_t>(record, 30), 65535U);
        EXPECT_EQ(at<std::uint16_t>(record, 34), 65535U);
    }
}

// The position along a gradient follows the text as it turns and stretches, so the gradient keeps its place in the letters.
TEST_F(TextPainterTest, TurnsAndStretchesTheGradientWithTheText) {
    text::Font& font = *fixture.engine().getDefaultFont();
    const std::array<math::Color, 2> colors{math::Color::white(), math::Color::black()};
    const text::Style style{.size = 32.0F, .gradient = text::Gradient::fromColors(colors, 0.0F), .anchor = {0.5F, 0.5F}, .rotation = 0.7F, .scale = {2.0F, 0.5F}};
    const std::shared_ptr<const text::Layout> layout = font.layout("Wide", style);
    TextPainter painter = makePainter();
    painter.paintText(*layout, {300.0F, 200.0F}, style, {});
    const GpuInstance& record = painter.getBatches()[0].instances[1];

    // clang-format off
    const auto place = [&](math::Vec2 local) {
        const math::Vec2 scaled = (local - layout->size * style.anchor) * style.scale;
        return math::Vec2{300.0F + scaled.x * std::cos(0.7F) - scaled.y * std::sin(0.7F), 200.0F + scaled.x * std::sin(0.7F) + scaled.y * std::cos(0.7F)};
    };
    // clang-format on
    const math::Rect& box = layout->lines[0].box;
    EXPECT_NEAR(positionAlong(record, place({box.x, box.y})), 0.0F, 0.0001F);
    EXPECT_NEAR(positionAlong(record, place({box.x, box.getBottom()})), 0.0F, 0.0001F);
    EXPECT_NEAR(positionAlong(record, place(box.getCenter())), 0.5F, 0.0001F);
    EXPECT_NEAR(positionAlong(record, place({box.getRight(), box.y})), 1.0F, 0.0001F);
}

// Rich text fills the glyphs of a gradient tag with it, over their outlines and beside plain glyphs, while bitmap glyphs keep their color.
TEST_F(TextPainterTest, FillsTheGlyphsOfAGradientTag) {
    text::RichText tagged = make("[outline=2][gradient=#FF0000,#0000FF offsets=0.2,0.6]G[/gradient][/outline]x", {.size = 32.0F});
    const std::vector<TextPainter::Batch> batches = paint(tagged);
    ASSERT_EQ(batches.size(), 3U);
    EXPECT_EQ(batches[0].program, Program::Text);
    EXPECT_EQ(batches[0].instances.size(), 1U);
    EXPECT_EQ(batches[1].program, Program::GradientText);
    ASSERT_EQ(batches[1].instances.size(), 2U);
    EXPECT_EQ(at<std::uint16_t>(batches[1].instances[1], 28), static_cast<std::uint16_t>(0.2F * 65535.0F + 0.5F));
    EXPECT_EQ(at<std::uint16_t>(batches[1].instances[1], 30), static_cast<std::uint16_t>(0.6F * 65535.0F + 0.5F));
    EXPECT_EQ(batches[2].program, Program::Text);
    EXPECT_EQ(batches[2].instances.size(), 1U);

    const graphics::Texture page = fixture.engine().getGraphics().createTexture(graphics::Image(16, 8, math::Color::white()));
    const std::shared_ptr<text::Font> grid = std::make_shared<text::BitmapFont>(text::BitmapFont::describeGrid({.characters = "AB", .cellWidth = 8.0F, .cellHeight = 8.0F}, page.getSize()), std::vector<graphics::Texture>{page});
    const std::array<math::Color, 2> colors{math::Color::white(), math::Color::black()};
    const text::Style style{.size = 16.0F, .gradient = text::Gradient::fromColors(colors)};
    TextPainter pixel = makePainter();
    pixel.paintText(*grid->layout("AB", style), {}, style, {});
    ASSERT_EQ(pixel.getBatches().size(), 1U);
    EXPECT_EQ(pixel.getBatches()[0].program, Program::Sprite);
    EXPECT_EQ(pixel.getBatches()[0].instances.size(), 2U);
}

} // namespace haylen::graphics2d
