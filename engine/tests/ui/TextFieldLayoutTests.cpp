#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Utf8.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "support/EngineFixture.hpp"
#include "support/FontCollection.hpp"
#include "ui/TextFieldLayout.hpp"

namespace haylen::ui {

// Lays out the text of a field in the Thai test font with lines far lower than its stacked marks, so its lines take different heights.
class TextFieldLayoutTest : public ::testing::Test {
  protected:
    test::EngineFixture fixture;
    std::shared_ptr<text::Font> thai = std::make_shared<text::TrueTypeFont>(fixture.engine().getGraphics(), test::FontCollection::setLineMetrics(test::FontCollection::read("noto-sans-thai-regular.ttf"), 300, -100, 0));
    std::string value = "กา\nที่ดี";
    std::u32string codePoints = core::Utf8::decode(value);
};

// The caret, the boxes of a selection and the points that place the caret follow the box of each line, the taller line of stacked marks and the shorter one alike, and every line of a uniform field.
TEST_F(TextFieldLayoutTest, FollowsTheHeightOfEachLine) {
    for (const text::LineHeight lineHeight : {text::LineHeight::Natural, text::LineHeight::Uniform}) {
        const TextFieldLayout field(thai->layout(value, {.size = 40.0F, .lineHeight = lineHeight, .language = "th"}), codePoints);
        const text::Layout& laid = field.getLayout();
        ASSERT_EQ(laid.lines.size(), 2U);
        EXPECT_EQ(laid.lines[0].box.height < laid.lines[1].box.height, lineHeight == text::LineHeight::Natural);

        std::vector<math::Rect> boxes;
        field.getSelection(0, codePoints.size(), boxes);
        ASSERT_EQ(boxes.size(), 2U);

        for (std::size_t index = 0; index < laid.lines.size(); ++index) {
            const text::Layout::Line& line = laid.lines[index];
            const math::Rect caret = field.getCaret(line.begin);
            EXPECT_FLOAT_EQ(caret.y, line.box.y);
            EXPECT_FLOAT_EQ(caret.height, line.box.height);
            EXPECT_FLOAT_EQ(boxes[index].y, line.box.y);
            EXPECT_FLOAT_EQ(boxes[index].height, line.box.height);

            // A point near the top or the bottom of a line places the caret on that line.
            for (const float y : {line.box.y + 0.5F, line.box.getBottom() - 0.5F}) {
                const std::size_t hit = field.hitTest({line.box.x + 1.0F, y});
                EXPECT_GE(hit, line.begin) << "Line " << index << " at " << y << ".";
                EXPECT_LE(hit, line.end) << "Line " << index << " at " << y << ".";
            }
        }

        EXPECT_GE(field.moveAlong(0, true), laid.lines[1].begin);
    }
}

} // namespace haylen::ui
