#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/ui/Backend.hpp"
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

// A label that does not wrap draws a text that is too wide smaller, down to its smallest scale, and shortens it only below that scale.
TEST_F(LabelTest, ShrinksATextThatIsTooWideBeforeItShortensIt) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "label", "id": "title", "wrap": false, "width": 200, "minScale": 0.4, "text": "A long title of the general store of the island"}]})");
    const float line = getUi().getTheme().getFont(Theme::Font::Body).size;
    const math::Rect box = getBounds(*gui, "title");
    const float spread = line * 0.2F;

    const std::vector<math::Rect> shrunk = drawText();
    ASSERT_EQ(shrunk.size(), 1U);
    EXPECT_LT(shrunk.front().height, line * 0.9F);
    EXPECT_GT(shrunk.front().width, box.width * 0.8F);
    EXPECT_LE(shrunk.front().getRight(), box.getRight() + spread);

    // At the full size the same text no longer fits and ends with an ellipsis at the height of its font.
    gui->set("title", {{"minScale", 1}});
    const std::vector<math::Rect> shortened = drawText();
    ASSERT_EQ(shortened.size(), 1U);
    EXPECT_GT(shortened.front().height, line * 0.9F);
    EXPECT_LE(shortened.front().getRight(), box.getRight() + spread);
}

// A label whose wrapped lines are taller than its height draws them smaller, down to its smallest scale, until they fit.
TEST_F(LabelTest, ShrinksWrappedLinesThatAreTallerThanTheLabel) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "label", "id": "story", "width": 300, "height": 90, "minScale": 0.3, "text": "The tide rose over the reef while the crew counted the coins of the old wreck and sang."}]})");
    const math::Rect box = getBounds(*gui, "story");
    const float spread = getUi().getTheme().getFont(Theme::Font::Body).size * 0.2F;

    const std::vector<math::Rect> fitted = drawText();
    ASSERT_EQ(fitted.size(), 1U);
    EXPECT_LE(fitted.front().height, box.height + spread * 2.0F);

    gui->set("story", {{"minScale", 1}});
    const std::vector<math::Rect> full = drawText();
    ASSERT_EQ(full.size(), 1U);
    EXPECT_GT(full.front().height, box.height + spread * 2.0F);
}

// The monospace role of the built-in themes draws with a monospace font, so every character takes the same width and columns of code and numbers line up.
TEST_F(LabelTest, DrawsTheMonospaceRoleWithAMonospaceFont) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "label", "id": "narrow", "font": "monospace", "text": "iiiiiiii"},
        {"kind": "label", "id": "wide", "font": "monospace", "text": "MMMMMMMM"},
        {"kind": "label", "id": "digits", "font": "monospace", "text": "11111111"}
    ]})");
    // clang-format on
    EXPECT_EQ(getUi().getTheme().getFont(Theme::Font::Monospace).font, Backend::kMonospaceFontName);
    EXPECT_EQ(getBounds(*gui, "narrow").width, getBounds(*gui, "wide").width);
    EXPECT_EQ(getBounds(*gui, "digits").width, getBounds(*gui, "wide").width);
}

// Check boxes, toggles, radio buttons and buttons wrap a label longer than their width onto more lines, as labels do, and grow taller to hold them, instead of cutting it with an ellipsis.
TEST_F(LabelTest, WrapsTheLabelsOfChoicesAndButtons) {
    const std::string text = "Limit the coin sound to four voices at once and drop the oldest";
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "column", "width": 420, "alignItems": "stretch", "children": [
        {"kind": "checkbox", "id": "check", "text": ")" + text + R"("},
        {"kind": "toggle", "id": "toggle", "text": ")" + text + R"("},
        {"kind": "radioGroup", "id": "radio", "items": [{"id": "one", "text": ")" + text + R"("}]},
        {"kind": "button", "id": "button", "text": ")" + text + R"("}
    ]}]})");
    // clang-format on
    frames(2);
    const float control = getUi().getTheme().getMetric(Theme::Metric::ControlHeight);
    for (const char* id : {"check", "toggle", "radio", "button"}) {
        EXPECT_GT(getBounds(*gui, id).height, control) << id;
        EXPECT_LE(getBounds(*gui, id).width, 420.0F) << id;
    }
    Backend& backend = getUi().getBackend();
    int whole = 0;
    for (const Backend::Text& drawn : backend.getTexts()) {
        whole += backend.getContent(drawn) == text ? 1 : 0;
    }
    EXPECT_EQ(whole, 4);
}

} // namespace haylen::ui
