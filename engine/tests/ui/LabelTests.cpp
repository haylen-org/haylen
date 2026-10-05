#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Theme.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

class LabelTest : public ::testing::Test, public test::UiFixture {
  protected:
    // Runs a frame and returns the bounds of the blocks of text the renderer drew on the screen, in UI coordinates.
    [[nodiscard]] std::vector<math::Rect> drawText() {
        std::vector<math::Rect> blocks;
        graphics2d::Renderer& renderer = getEngine().getRenderer2D();
        // clang-format off
        const std::uint64_t overlay = renderer.addCanvasOverlay([&blocks](graphics2d::Renderer& drawing) {
            drawing.visitDrawn([&blocks](const graphics2d::Renderer::Drawn& drawn) {
                if (drawn.text) {
                    blocks.push_back(math::Geometry::bounds(drawn.corners));
                }
            });
        });
        // clang-format on
        frames();
        renderer.removeCanvasOverlay(overlay);
        for (math::Rect& block : blocks) {
            block = block.translated(-getEngine().getViewport().getVisibleRect().getMin());
        }
        return blocks;
    }
};

} // namespace

// A label that does not wrap keeps the line breaks of its text, shortens each line to its width and centers the block of lines in its height, instead of drawing every line from a box of one line.
TEST_F(LabelTest, KeepsTheLinesOfATextThatDoesNotWrapInsideTheLabel) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "label", "id": "lines", "wrap": false, "width": 260, "text": "The first line is far too long for the label\nSecond\nThird"}]})");
    const float line = getUi().getTheme().getFont(Theme::Font::Body).size;
    const math::Rect box = getBounds(*gui, "lines");
    EXPECT_GE(box.height, line * 3.0F);

    const std::vector<math::Rect> blocks = drawText();
    ASSERT_EQ(blocks.size(), 1U);
    const math::Rect& text = blocks.front();
    EXPECT_GT(text.height, line * 2.5F);
    EXPECT_NEAR(text.getCenter().y, box.getCenter().y, line * 0.25F);
    // Glyph quads reach a little past the letters, by the spread of their distance field.
    const float spread = line * 0.2F;
    EXPECT_GE(text.x, box.x - spread);
    EXPECT_LE(text.getRight(), box.getRight() + spread);

    // In a taller box the block of lines stays in the middle.
    gui->set("lines", {{"height", 400}});
    const std::vector<math::Rect> centered = drawText();
    ASSERT_EQ(centered.size(), 1U);
    EXPECT_NEAR(centered.front().getCenter().y, getBounds(*gui, "lines").getCenter().y, line * 0.25F);
}

} // namespace haylen::ui
