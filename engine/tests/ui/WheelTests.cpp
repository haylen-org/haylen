#include <gtest/gtest.h>

#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/ui/Gui.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

class WheelTest : public ::testing::Test, public test::UiFixture {
  protected:
    [[nodiscard]] static std::string labels(int count) {
        std::string list;
        for (int index = 1; index <= count; ++index) {
            list += (index > 1 ? ", " : "") + std::string(R"({"kind": "label", "text": "Line )") + std::to_string(index) + R"("})";
        }
        return list;
    }

    // Scrolls over an area, waits until the gesture ends and the content rests, and returns how far the content moved up.
    float scrolled(const Gui& gui, std::string_view area, std::string_view content, float delta, bool precise) {
        const float before = getBounds(gui, content).y;
        wheel(getBounds(gui, area).getCenter(), delta, 0.0F, precise);
        frames(60);
        return before - getBounds(gui, content).y;
    }
};

} // namespace

TEST_F(WheelTest, WheelsScrollByLinesOfTheThemeAndTrackpadsByTheirPoints) {
    // A step of a wheel scrolls by `scrollLine` of the theme or the style of the area, and a trackpad by the points it moved, which are UI units on this screen.
    auto gui = mount(R"({"kind": "row", "children": [
        {"kind": "scroll", "id": "plain", "width": 400, "height": 300, "children": [{"kind": "column", "id": "plainContent", "children": [)" +
                     labels(60) + R"(]}]},
        {"kind": "scroll", "id": "styled", "width": 400, "height": 300, "style": {"metrics": {"scrollLine": 20}}, "children": [{"kind": "column", "id": "styledContent", "children": [)" +
                     labels(60) + R"(]}]}
    ]})");
    ASSERT_FLOAT_EQ(getUi().getBackend().getPointsPerUnit(), 1.0F);
    EXPECT_NEAR(scrolled(*gui, "plain", "plainContent", -1.0F, false), 64.0F, 0.5F);
    EXPECT_NEAR(scrolled(*gui, "plain", "plainContent", -30.0F, true), 30.0F, 0.5F);
    EXPECT_NEAR(scrolled(*gui, "plain", "plainContent", 2.5F, true), -2.5F, 0.5F);
    EXPECT_NEAR(scrolled(*gui, "styled", "styledContent", -2.0F, false), 40.0F, 0.5F);

    // The scroll speed of the app multiplies both.
    getEngine().getInput().setScrollSpeed(0.5F);
    EXPECT_NEAR(scrolled(*gui, "plain", "plainContent", -1.0F, false), 32.0F, 0.5F);
    EXPECT_NEAR(scrolled(*gui, "plain", "plainContent", -30.0F, true), 15.0F, 0.5F);
}

TEST_F(WheelTest, TheInnermostAreaThatCanScrollTakesTheWheel) {
    // An area inside another takes the wheel while it has more to show, and one that shows all it holds leaves the wheel to the area around it.
    auto gui = mount(R"({"kind": "scroll", "id": "page", "width": 600, "height": 400, "children": [{"kind": "column", "id": "pageContent", "children": [
        {"kind": "scroll", "id": "long", "width": 500, "height": 200, "children": [{"kind": "column", "id": "longContent", "children": [)" +
                     labels(40) + R"(]}]},
        {"kind": "scroll", "id": "short", "width": 500, "height": 200, "children": [{"kind": "column", "id": "shortContent", "children": [)" +
                     labels(2) + R"(]}]},
        )" + labels(30) +
                     R"(
    ]}]})");
    const float page = getBounds(*gui, "pageContent").y;
    EXPECT_NEAR(scrolled(*gui, "long", "longContent", -1.0F, false), 64.0F, 0.5F);
    EXPECT_EQ(getBounds(*gui, "pageContent").y, page);
    EXPECT_NEAR(scrolled(*gui, "short", "pageContent", -1.0F, false), 64.0F, 0.5F);
}

} // namespace haylen::ui
