#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "2d/graphics/GpuInstance.hpp"
#include "2d/graphics/TextPainter.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/text/BitmapFont.hpp"
#include "haylen/text/RichText.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::graphics2d {

namespace {

// Decodes a parameter byte the way the shaders read a signed one.
int signedByte(std::uint8_t value) {
    return value < 128 ? value : value - 256;
}

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
        TextPainter painter(fixture.engine().getGraphics().getWhiteTexture());
        painter.paintRichText(richText.getFrame(), {100.0F, 50.0F});
        return painter.getBatches();
    }

    test::EngineFixture fixture;
};

} // namespace

TEST_F(TextPainterTest, PacksSyntheticStylesIntoTheGlyphParameters) {
    text::RichText styled = make("[b]b[/b][i]i[/i][outline=2 color=red]o[/outline]", {.size = 32.0F});
    const std::vector<TextPainter::Batch> batches = paint(styled);
    ASSERT_EQ(batches.size(), 1U);
    EXPECT_EQ(batches[0].program, Program::Text);
    ASSERT_EQ(batches[0].instances.size(), 3U);

    const GpuInstance& bold = batches[0].instances[0];
    const text::Font& font = *styled.getLayout().looks[0].font;
    EXPECT_NEAR(static_cast<float>(signedByte(bold.parameters[1])) / 256.0F, font.toDistance(32.0F * 0.03F, 32.0F), 1.0F / 256.0F);
    EXPECT_EQ(bold.parameters[2], 0U);
    EXPECT_EQ(bold.parameters[0], 0U);

    // Italics lean around the baseline, where their pivot sits.
    const GpuInstance& italic = batches[0].instances[1];
    EXPECT_EQ(signedByte(italic.parameters[2]), static_cast<int>(std::lround(0.2F * 127.0F)));
    EXPECT_EQ(italic.parameters[1], 0U);
    const text::TextLayout::Glyph& leaning = styled.getLayout().glyphs[1];
    EXPECT_FLOAT_EQ(italic.position[1], 50.0F + leaning.baseline);
    EXPECT_NEAR(italic.position[1] - italic.pivot[1] * italic.size[1], 50.0F + leaning.position.y, 0.001F);

    const GpuInstance& outlined = batches[0].instances[2];
    EXPECT_NEAR(static_cast<float>(outlined.parameters[0]) / 255.0F * 0.5F, font.toDistance(2.0F, 32.0F), 0.5F / 255.0F);
    EXPECT_EQ(outlined.flash, math::Color::fromHex(0xFF0000FFU).toRgba8());
}

TEST_F(TextPainterTest, ScalesAndTintsRichTextFromItsCorner) {
    text::RichText boxed = make("[bgcolor=white]ab[/bgcolor]", {.size = 20.0F});
    TextPainter painter(fixture.engine().getGraphics().getWhiteTexture());
    painter.paintRichText(boxed.getLayout(), {10.0F, 20.0F}, {2.0F, 3.0F}, math::Color{1.0F, 1.0F, 1.0F, 0.5F});
    ASSERT_EQ(painter.getBatches().size(), 2U);
    const GpuInstance& background = painter.getBatches()[0].instances[0];
    const math::Rect& box = boxed.getLayout().boxes[0].rect;
    EXPECT_FLOAT_EQ(background.position[0], 10.0F + box.x * 2.0F);
    EXPECT_FLOAT_EQ(background.size[1], box.height * 3.0F);
    EXPECT_NEAR(static_cast<double>(background.color >> 24U), 127.5, 1.0);

    const GpuInstance& glyph = painter.getBatches()[1].instances[0];
    const text::TextLayout::Glyph& laid = boxed.getLayout().glyphs[0];
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
    EXPECT_GT(signedByte(glow.parameters[1]), 0);
    EXPECT_GT(glow.parameters[3], 0U);
    EXPECT_EQ(glow.color, math::Color::fromHex(0xFFFF00FFU).toRgba8());
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

    TextPainter plain(fixture.engine().getGraphics().getWhiteTexture());
    const text::TextStyle style{.size = 16.0F, .shadowOffset = {1.0F, 1.0F}, .shadowColor = math::Color::black(), .shadowBlur = 4.0F};
    plain.paintText(*grid->layout("AB", style), {}, style);
    ASSERT_EQ(plain.getBatches().size(), 1U);
    EXPECT_EQ(plain.getBatches()[0].program, Program::Sprite);
    EXPECT_EQ(plain.getBatches()[0].instances[0].parameters[3], 0U);

    text::Font& field = *fixture.engine().getDefaultFont();
    TextPainter smooth(fixture.engine().getGraphics().getWhiteTexture());
    smooth.paintText(*field.layout("AB", style), {}, style);
    ASSERT_EQ(smooth.getBatches().size(), 1U);
    EXPECT_EQ(smooth.getBatches()[0].program, Program::Text);
    EXPECT_GT(smooth.getBatches()[0].instances[0].parameters[3], 0U);
    EXPECT_EQ(smooth.getBatches()[0].instances[2].parameters[3], 0U);
}

} // namespace haylen::graphics2d
