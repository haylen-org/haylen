#include <gtest/gtest.h>

#include <array>
#include <string>
#include <utility>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Gui.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

class LayoutTest : public ::testing::Test, public test::UiFixture {};

} // namespace

TEST_F(LayoutTest, DistributesFreeSpaceWithEveryJustify) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "row", "id": "row", "width": 1000, "height": 100, "gap": 0, "children": [
        {"kind": "spacer", "id": "a", "width": 100, "height": 100},
        {"kind": "spacer", "id": "b", "width": 100, "height": 100},
        {"kind": "spacer", "id": "c", "width": 100, "height": 100}
    ]}]})");
    // clang-format on
    const std::array<std::pair<std::string, std::array<float, 3>>, 6> expected{{
        {"start", {0.0F, 100.0F, 200.0F}},
        {"center", {350.0F, 450.0F, 550.0F}},
        {"end", {700.0F, 800.0F, 900.0F}},
        {"spaceBetween", {0.0F, 450.0F, 900.0F}},
        {"spaceAround", {117.0F, 450.0F, 783.0F}},
        {"spaceEvenly", {175.0F, 450.0F, 725.0F}},
    }};
    for (const auto& [justify, places] : expected) {
        gui->set("row", {{"justify", justify}});
        frames();
        EXPECT_EQ(getBounds(*gui, "a").x, places[0]) << justify;
        EXPECT_EQ(getBounds(*gui, "b").x, places[1]) << justify;
        EXPECT_EQ(getBounds(*gui, "c").x, places[2]) << justify;
    }
}

TEST_F(LayoutTest, AlignsChildrenWithAlignItemsUnlessTheyAlignThemselves) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "row", "height": 300, "alignItems": "end", "gap": 0, "children": [
        {"kind": "spacer", "id": "low", "width": 100, "height": 100},
        {"kind": "spacer", "id": "high", "width": 100, "height": 100, "align": "start"},
        {"kind": "spacer", "id": "tall", "width": 100, "align": "stretch", "maxHeight": 250}
    ]}]})");
    // clang-format on
    EXPECT_EQ(getBounds(*gui, "low").y, 200.0F);
    EXPECT_EQ(getBounds(*gui, "high").y, 0.0F);
    EXPECT_EQ(getBounds(*gui, "tall"), (math::Rect{200.0F, 0.0F, 100.0F, 250.0F}));
}

TEST_F(LayoutTest, GrowsChildrenWithinTheirMinimumAndMaximumSizes) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "row", "width": 1000, "height": 50, "gap": 0, "children": [
            {"kind": "spacer", "id": "capped", "grow": 1, "maxWidth": 200},
            {"kind": "spacer", "id": "rest", "grow": 1},
            {"kind": "spacer", "id": "fixed", "width": 100}
        ]},
        {"kind": "row", "width": 1000, "height": 50, "gap": 0, "children": [
            {"kind": "spacer", "id": "wide", "grow": 1, "minWidth": 600},
            {"kind": "spacer", "id": "narrow", "grow": 1}
        ]},
        {"kind": "row", "width": 1000, "height": 50, "gap": 0, "children": [
            {"kind": "spacer", "id": "one", "grow": 1},
            {"kind": "spacer", "id": "three", "grow": 3}
        ]}
    ]})");
    // clang-format on
    EXPECT_EQ(getBounds(*gui, "capped").width, 200.0F);
    EXPECT_EQ(getBounds(*gui, "rest").width, 700.0F);
    EXPECT_EQ(getBounds(*gui, "fixed").x, 900.0F);
    EXPECT_EQ(getBounds(*gui, "wide").width, 600.0F);
    EXPECT_EQ(getBounds(*gui, "narrow").width, 400.0F);
    EXPECT_EQ(getBounds(*gui, "one").width, 250.0F);
    EXPECT_EQ(getBounds(*gui, "three").width, 750.0F);
}

// A node with a fixed size of zero and a grow factor takes the room the others leave, such as a scroll that fills a panel under its title.
TEST_F(LayoutTest, GrowsANodeWhoseFixedSizeIsZero) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "column", "height": 500, "gap": 0, "children": [
        {"kind": "label", "id": "title", "text": "Title"},
        {"kind": "scroll", "id": "scroll", "grow": 1, "height": 0, "children": [{"kind": "column", "children": [{"kind": "label", "id": "inside", "text": "Inside"}]}]}
    ]}]})");
    // clang-format on
    frames(2);
    EXPECT_EQ(getBounds(*gui, "scroll").height, 500.0F - getBounds(*gui, "title").height);
    EXPECT_FALSE(getBounds(*gui, "inside").isEmpty());
}

// A scroll stretches its child across the other axis within the size bounds of the child, such as a form that keeps its maximum width on a wide screen, or places it at the alignment the child sets.
TEST_F(LayoutTest, BoundsAndAlignsTheChildOfAScroll) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "gap": 0, "children": [
        {"kind": "scroll", "id": "page", "width": 1000, "height": 200, "children": [{"kind": "column", "id": "form", "maxWidth": 600, "children": [{"kind": "spacer", "height": 50}]}]},
        {"kind": "scroll", "id": "centered", "width": 1000, "height": 200, "children": [{"kind": "column", "id": "card", "width": 400, "align": "center", "children": [{"kind": "spacer", "height": 50}]}]},
        {"kind": "scroll", "id": "shelf", "axis": "horizontal", "scrollbar": false, "width": 300, "height": 200, "children": [{"kind": "row", "id": "covers", "maxHeight": 120, "children": [{"kind": "spacer", "width": 800, "height": 100}]}]},
        {"kind": "scroll", "id": "strip", "axis": "horizontal", "scrollbar": false, "width": 300, "height": 200, "children": [{"kind": "row", "id": "tall", "children": [{"kind": "spacer", "width": 800, "height": 100}]}]}
    ]})");
    // clang-format on
    frames(2);
    EXPECT_EQ(getBounds(*gui, "form"), (math::Rect{getBounds(*gui, "page").x, getBounds(*gui, "page").y, 600.0F, 50.0F}));
    EXPECT_EQ(getBounds(*gui, "card").x, getBounds(*gui, "centered").x + 300.0F);
    EXPECT_EQ(getBounds(*gui, "card").width, 400.0F);
    EXPECT_EQ(getBounds(*gui, "covers").height, 120.0F);
    EXPECT_EQ(getBounds(*gui, "tall").height, 200.0F);
}

TEST_F(LayoutTest, KeepsMarginsAroundChildrenAndPaddingInsideContainers) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": [5, 6, 7, 8], "gap": 4, "children": [
        {"kind": "spacer", "id": "first", "width": 100, "height": 50, "margin": [10, 20, 30, 40]},
        {"kind": "spacer", "id": "second", "width": 100, "height": 50},
        {"kind": "row", "id": "row", "gap": 0, "children": [
            {"kind": "spacer", "id": "left", "width": 100, "height": 50, "margin": [0, 12]},
            {"kind": "spacer", "id": "right", "width": 100, "height": 50}
        ]}
    ]})");
    // clang-format on
    EXPECT_EQ(getBounds(*gui, "first"), (math::Rect{8.0F + 40.0F, 5.0F + 10.0F, 100.0F, 50.0F}));
    EXPECT_EQ(getBounds(*gui, "second").y, 5.0F + 10.0F + 50.0F + 30.0F + 4.0F);
    EXPECT_EQ(getBounds(*gui, "left").x, 8.0F + 12.0F);
    EXPECT_EQ(getBounds(*gui, "right").x, 8.0F + 12.0F + 100.0F + 12.0F);
    EXPECT_EQ(getBounds(*gui, "row").width, 1920.0F - 8.0F - 6.0F);
}

TEST_F(LayoutTest, WrapsRowsIntoLines) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "row", "id": "row", "width": 500, "wrap": true, "gap": 10, "lineGap": 20, "justify": "center", "children": [
        {"kind": "spacer", "id": "a", "width": 150, "height": 40},
        {"kind": "spacer", "id": "b", "width": 150, "height": 40},
        {"kind": "spacer", "id": "c", "width": 150, "height": 40},
        {"kind": "spacer", "id": "d", "width": 150, "height": 40},
        {"kind": "spacer", "id": "e", "width": 150, "height": 60}
    ]}]})");
    // clang-format on
    EXPECT_EQ(getBounds(*gui, "a").getMin(), math::Vec2(15.0F, 0.0F));
    EXPECT_EQ(getBounds(*gui, "c").x, 15.0F + 320.0F);
    EXPECT_EQ(getBounds(*gui, "d").getMin(), math::Vec2(95.0F, 70.0F));
    EXPECT_EQ(getBounds(*gui, "e").getMin(), math::Vec2(255.0F, 60.0F));
    EXPECT_EQ(getBounds(*gui, "row").height, 40.0F + 20.0F + 60.0F);
    EXPECT_THROW(gui->set("row", {{"wrap", "yes"}}), std::invalid_argument);
}

TEST_F(LayoutTest, PlacesGridCellsAndAlignsChildrenInBothDirections) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "grid", "id": "grid", "width": 420, "columns": 2, "gap": 20, "rowGap": 30, "children": [
        {"kind": "spacer", "id": "tall", "height": 100},
        {"kind": "spacer", "id": "small", "width": 50, "height": 40, "align": "center"},
        {"kind": "spacer", "id": "corner", "width": 50, "height": 40, "align": "end"},
        {"kind": "spacer", "id": "filled", "height": 80}
    ]}]})");
    // clang-format on
    EXPECT_EQ(getBounds(*gui, "tall"), (math::Rect{0.0F, 0.0F, 200.0F, 100.0F}));
    EXPECT_EQ(getBounds(*gui, "small"), (math::Rect{220.0F + 75.0F, 30.0F, 50.0F, 40.0F}));
    EXPECT_EQ(getBounds(*gui, "corner"), (math::Rect{150.0F, 130.0F + 40.0F, 50.0F, 40.0F}));
    EXPECT_EQ(getBounds(*gui, "filled"), (math::Rect{220.0F, 130.0F, 200.0F, 80.0F}));
    EXPECT_EQ(getBounds(*gui, "grid").height, 100.0F + 30.0F + 80.0F);
}

TEST_F(LayoutTest, FitsGridColumnsToTheMinimumColumnWidth) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "grid", "id": "grid", "width": 1000, "minColumnWidth": 300, "gap": 20, "children": [
        {"kind": "spacer", "id": "a", "height": 50}, {"kind": "spacer", "id": "b", "height": 50}, {"kind": "spacer", "id": "c", "height": 50}
    ]}]})");
    // clang-format on
    EXPECT_EQ(getBounds(*gui, "c").y, 0.0F);
    EXPECT_EQ(getBounds(*gui, "a").width, (1000.0F - 40.0F) / 3.0F);

    gui->set("grid", {{"width", 900}});
    frames();
    EXPECT_EQ(getBounds(*gui, "c").getMin(), math::Vec2(0.0F, 70.0F));
    EXPECT_EQ(getBounds(*gui, "a").width, 440.0F);

    gui->set("grid", {{"width", 1000}, {"columns", 2}});
    frames();
    EXPECT_EQ(getBounds(*gui, "c").y, 70.0F);
}

TEST_F(LayoutTest, StacksChildrenAndPlacesThemInBothDirections) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "stack", "width": 400, "height": 300, "alignItems": "center", "children": [
        {"kind": "spacer", "id": "middle", "width": 100, "height": 100},
        {"kind": "spacer", "id": "corner", "width": 100, "height": 100, "align": "end"},
        {"kind": "spacer", "id": "band", "align": "stretch", "maxWidth": 200, "minHeight": 400}
    ]}]})");
    // clang-format on
    EXPECT_EQ(getBounds(*gui, "middle"), (math::Rect{150.0F, 100.0F, 100.0F, 100.0F}));
    EXPECT_EQ(getBounds(*gui, "corner"), (math::Rect{300.0F, 200.0F, 100.0F, 100.0F}));
    EXPECT_EQ(getBounds(*gui, "band"), (math::Rect{0.0F, 0.0F, 200.0F, 300.0F}));
}

TEST_F(LayoutTest, KeepsTheAspectRatio) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "column", "width": 800, "children": [{"kind": "spacer", "id": "wide", "aspectRatio": 2}]},
        {"kind": "spacer", "id": "high", "height": 100, "aspectRatio": 0.5},
        {"kind": "stack", "width": 400, "height": 400, "children": [{"kind": "spacer", "id": "fitted", "align": "stretch", "aspectRatio": 2}]}
    ]})");
    // clang-format on
    EXPECT_EQ(getBounds(*gui, "wide").getSize(), math::Vec2(800.0F, 400.0F));
    EXPECT_EQ(getBounds(*gui, "high").getSize(), math::Vec2(50.0F, 100.0F));
    const math::Rect stack = getBounds(*gui, "fitted");
    EXPECT_EQ(stack.getSize(), math::Vec2(400.0F, 200.0F));
    EXPECT_EQ(stack.y, getBounds(*gui, "high").getBottom() + 16.0F + 100.0F);
    EXPECT_THROW(gui->set("high", {{"aspectRatio", 0}}), std::invalid_argument);
}

} // namespace haylen::ui
