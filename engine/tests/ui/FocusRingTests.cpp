#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Theme.hpp"
#include "support/TestFiles.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

// A UI with an image for the focus frame of a theme, whose ring the shapes of the backend show.
class FocusRingTest : public ::testing::Test, public test::UiFixture {
  protected:
    FocusRingTest() : UiFixture({{"content/ui/frame.png", toText(test::TestFiles::pngImage(12, 12, 0xFFFFFFFFU))}}) {}

    [[nodiscard]] static std::string toText(const std::vector<std::uint8_t>& bytes) {
        return {bytes.begin(), bytes.end()};
    }

    [[nodiscard]] float getMetric(Theme::Metric role) {
        return getUi().getTheme().getMetric(role);
    }

    [[nodiscard]] bool drawsRing() {
        const math::Color focus = getUi().getTheme().getColor(Theme::Color::Focus);
        const std::span<const graphics2d::Shape> shapes = getUi().getBackend().getShapes();
        return std::ranges::any_of(shapes, [focus](const graphics2d::Shape& shape) { return shape.borderColor == focus && shape.borderWidth > 0.0F; });
    }
};

} // namespace

// A node that turns the ring off leaves it out for itself and its children, so the app draws the focus its own way, while the focus still moves to them.
TEST_F(FocusRingTest, LeavesTheRingOutOfNodesThatDrawTheirOwnFocus) {
    const std::shared_ptr<Gui> gui = mount(R"({"kind": "column", "children": [{"kind": "button", "id": "play", "text": "Play", "autofocus": true}, {"kind": "column", "focusRing": false, "children": [{"kind": "button", "id": "quit", "text": "Quit"}]}]})");
    key(input::Key::Down);
    frames(1);
    key(input::Key::Up);
    frames(1);
    EXPECT_TRUE(drawsRing());

    key(input::Key::Down);
    frames(2);
    EXPECT_TRUE(isFocused(*gui, "quit"));
    EXPECT_FALSE(drawsRing());
}

// A theme with a focus ring surface frames the focused control with its image where the ring would run, instead of the line.
TEST_F(FocusRingTest, FramesTheFocusWithTheImageOfTheTheme) {
    const std::shared_ptr<Gui> gui = mount(R"({"kind": "column", "padding": 40, "style": {"surfaces": {"focusRing": {"image": "ui/frame.png", "slice": 4}}}, "children": [{"kind": "button", "id": "play", "text": "Play", "autofocus": true}]})");
    key(input::Key::Right);
    std::vector<math::Rect> frame;
    bool framing = false;
    graphics2d::Renderer& renderer = getEngine().getRenderer2D();
    // clang-format off
    const std::uint64_t overlay = renderer.addCanvasOverlay([&](graphics2d::Renderer& drawing) {
        frame.clear();
        drawing.visitDrawn([&](const graphics2d::Renderer::Drawn& drawn) {
            if (drawn.text || !drawn.label.empty()) {
                framing = drawn.label == "ui/frame.png";
            }
            if (framing) {
                frame.push_back(math::Geometry::bounds(drawn.corners));
            }
        });
    });
    // clang-format on

    // The image loads on the worker threads, so the frames go on until the frame draws with it.
    ASSERT_TRUE(getFixture().frameUntil([&] { return !frame.empty(); }));
    frames(1);
    renderer.removeCanvasOverlay(overlay);
    math::Rect covered = frame.front();
    for (const math::Rect& piece : frame) {
        covered = covered.merged(piece);
    }
    const float reach = getMetric(Theme::Metric::FocusGap) + getMetric(Theme::Metric::FocusWidth);
    const math::Rect expected = getBounds(*gui, "play").expanded(reach);
    EXPECT_NEAR(covered.x, expected.x, 1.0F);
    EXPECT_NEAR(covered.getRight(), expected.getRight(), 1.0F);
    EXPECT_FALSE(drawsRing());
}

} // namespace haylen::ui
