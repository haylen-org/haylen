#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Theme.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

class LayoutTest : public ::testing::Test, public test::UiFixture {
  protected:
    // Whether a draw of the last frame is cut to the area.
    [[nodiscard]] static bool hasClip(const math::Rect& area) {
        for (const ImDrawList* list : ImGui::GetDrawData()->CmdLists) {
            for (const ImDrawCmd& command : list->CmdBuffer) {
                const math::Rect clip = math::Rect::fromMinMax({command.ClipRect.x, command.ClipRect.y}, {command.ClipRect.z, command.ClipRect.w});
                if (command.UserCallback != nullptr && clip == area) {
                    return true;
                }
            }
        }
        return false;
    }

    // Whether the last frame drew a vertex of the color on the line across the screen at the height.
    [[nodiscard]] static bool hasVertex(math::Color color, float y) {
        const ImU32 packed = ImGui::ColorConvertFloat4ToU32({color.r, color.g, color.b, color.a});
        for (const ImDrawList* list : ImGui::GetDrawData()->CmdLists) {
            for (const ImDrawVert& vertex : list->VtxBuffer) {
                if (vertex.col == packed && std::fabs(vertex.pos.y - y) < 0.5F) {
                    return true;
                }
            }
        }
        return false;
    }
};

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

// A scroll that fills stretches a child shorter than its area to the whole area, so the children that grow share the room, and scrolls the child once it is longer.
TEST_F(LayoutTest, FillsTheAreaOfAScrollWithAShorterChild) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "scroll", "id": "page", "fill": true, "height": 500, "children": [{"kind": "column", "gap": 0, "children": [
        {"kind": "spacer", "id": "top", "height": 100},
        {"kind": "spacer", "id": "growing", "grow": 1},
        {"kind": "spacer", "id": "bottom", "height": 100}
    ]}]}]})");
    // clang-format on
    frames(2);
    EXPECT_EQ(getBounds(*gui, "growing").height, 300.0F);
    EXPECT_EQ(getBounds(*gui, "bottom").y, 400.0F);

    gui->set("top", {{"height", 800}});
    frames(2);
    EXPECT_EQ(getBounds(*gui, "growing").height, 0.0F);
    EXPECT_EQ(getBounds(*gui, "bottom").y, 800.0F);

    gui->set("page", {{"fill", false}});
    gui->set("top", {{"height", 100}});
    frames(2);
    EXPECT_EQ(getBounds(*gui, "bottom").y, 100.0F);
}

// An area that scrolls fades its content into the theme color along each edge where the content continues, and only there.
TEST_F(LayoutTest, FadesTheEdgesOfAScrollWhereTheContentContinues) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "scroll", "id": "area", "height": 300, "scrollbar": false, "children": [{"kind": "column", "children": [{"kind": "spacer", "height": 900}]}]}]})");
    frames(2);
    getUi().getBackend().makeCurrent();
    const math::Color fade = getUi().getTheme().getColor(Theme::Color::ScrollFade);
    const math::Rect area = getBounds(*gui, "area");
    EXPECT_TRUE(hasVertex(fade, area.getBottom()));
    EXPECT_FALSE(hasVertex(fade, area.y));

    gui->command(getUi().getContext(), "area", "scrollTo", core::Json{{"offset", 600}, {"animated", false}});
    frames(2);
    EXPECT_TRUE(hasVertex(fade, area.y));
    EXPECT_FALSE(hasVertex(fade, area.getBottom()));
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

// A child that grows stays on its line while its minimum width fits, however wide its content is, and takes the room the line leaves.
TEST_F(LayoutTest, WrapsAGrowingChildOnlyWhenItsMinimumWidthDoesNotFit) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "row", "id": "row", "width": 1000, "wrap": true, "gap": 20, "children": [
        {"kind": "spacer", "id": "fixed", "width": 400, "height": 100},
        {"kind": "column", "id": "growing", "grow": 1, "children": [{"kind": "label", "wrap": false, "text": "A line of text far wider than the room the first child leaves on the row, which would push it to a line of its own"}]}
    ]}]})");
    // clang-format on
    EXPECT_EQ(getBounds(*gui, "growing").x, 420.0F);
    EXPECT_EQ(getBounds(*gui, "growing").width, 580.0F);
    EXPECT_LT(getBounds(*gui, "growing").y, 100.0F);

    gui->set("growing", {{"minWidth", 700}});
    frames();
    EXPECT_EQ(getBounds(*gui, "growing").getMin(), math::Vec2(0.0F, 100.0F + 20.0F));
    EXPECT_EQ(getBounds(*gui, "growing").width, 1000.0F);
}

// The lines of a wrapped row share the height the row has beyond their own, so children that stretch fill it.
TEST_F(LayoutTest, StretchesTheLinesOfAWrappedRowOverItsHeight) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "row", "id": "row", "width": 500, "height": 400, "wrap": true, "gap": 10, "lineGap": 20, "children": [
        {"kind": "spacer", "id": "a", "width": 300, "height": 40, "align": "stretch"},
        {"kind": "spacer", "id": "b", "width": 300, "height": 60},
        {"kind": "column", "id": "c", "width": 300, "align": "stretch", "children": [{"kind": "spacer", "height": 20}]}
    ]}]})");
    // clang-format on
    const float extra = (400.0F - 40.0F - 60.0F - 20.0F - 20.0F * 2.0F) / 3.0F;
    EXPECT_EQ(getBounds(*gui, "a"), (math::Rect{0.0F, 0.0F, 300.0F, 40.0F}));
    EXPECT_EQ(getBounds(*gui, "b").height, 60.0F);
    EXPECT_EQ(getBounds(*gui, "c").y, std::floor(40.0F + extra + 20.0F + 60.0F + extra + 20.0F));
    EXPECT_NEAR(getBounds(*gui, "c").height, 20.0F + extra, 1.0F);
}

// Lines taller than their row and children taller than their panel are cut at its edges instead of drawing over what lies around it.
TEST_F(LayoutTest, ClipsWhatOverflowsPanelsAndWrappedRows) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 40, "gap": 40, "children": [
        {"kind": "panel", "id": "panel", "height": 150, "children": [{"kind": "label", "text": "Top"}, {"kind": "spacer", "height": 200}, {"kind": "label", "text": "Past the edge"}]},
        {"kind": "row", "id": "row", "width": 400, "height": 100, "wrap": true, "children": [
            {"kind": "label", "width": 300, "text": "First"}, {"kind": "spacer", "width": 300, "height": 120}, {"kind": "label", "width": 300, "text": "Third"}
        ]}
    ]})");
    // clang-format on
    frames(2);
    getUi().getBackend().makeCurrent();
    const float reach = getUi().getTheme().getMetric(Theme::Metric::FocusGap) + getUi().getTheme().getMetric(Theme::Metric::FocusWidth);
    EXPECT_TRUE(hasClip(getBounds(*gui, "panel")));
    EXPECT_TRUE(hasClip(getBounds(*gui, "row").expanded(reach)));
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

// A finger flings a scroll on past where it lets go.
TEST_F(LayoutTest, FlingsAScrollWithAFingerAndSpringsBackFromPastItsEnds) {
    auto gui = mount(R"({"kind": "scroll", "id": "page", "width": 600, "height": 400, "children": [{"kind": "column", "id": "content", "children": [{"kind": "spacer", "height": 4000}]}]})");
    const math::Vec2 center = getBounds(*gui, "page").getCenter();
    const float top = getBounds(*gui, "content").y;

    touch(platform::Event::Type::TouchBegan, 1, center + math::Vec2{0.0F, 150.0F});
    frames();
    for (int step = 1; step <= 6; ++step) {
        touch(platform::Event::Type::TouchMoved, 1, center + math::Vec2{0.0F, 150.0F - 50.0F * static_cast<float>(step)});
        frames();
    }
    const float released = getBounds(*gui, "content").y;
    EXPECT_LT(released, top - 150.0F);
    touch(platform::Event::Type::TouchEnded, 1, center - math::Vec2{0.0F, 150.0F});
    frames(120);
    EXPECT_LT(getBounds(*gui, "content").y, released - 300.0F);
}

// A finger that presses a text field of a list and then drags slowly scrolls the list, as on a phone, where only a tap edits the field.
TEST_F(LayoutTest, ScrollsAListThatAFingerDragsFromOneOfItsFields) {
    auto gui = mount(R"({"kind": "scroll", "id": "page", "width": 600, "height": 400, "children": [{"kind": "column", "id": "content", "children": [{"kind": "textField", "id": "name"}, {"kind": "spacer", "height": 4000}]}]})");
    const math::Vec2 field = getBounds(*gui, "name").getCenter();
    const float top = getBounds(*gui, "content").y;
    touch(platform::Event::Type::TouchBegan, 1, field);
    frames(10);
    for (int step = 1; step <= 20; ++step) {
        touch(platform::Event::Type::TouchMoved, 1, field - math::Vec2{0.0F, 4.0F * static_cast<float>(step)});
        frames();
    }
    EXPECT_LT(getBounds(*gui, "content").y, top - 40.0F);
    touch(platform::Event::Type::TouchEnded, 1, field - math::Vec2{0.0F, 80.0F});
    frames(2);
}

TEST_F(LayoutTest, StretchesAScrollPulledPastItsStartAndSpringsBack) {
    auto gui = mount(R"({"kind": "scroll", "id": "page", "width": 600, "height": 400, "children": [{"kind": "column", "id": "content", "children": [{"kind": "spacer", "height": 4000}]}]})");
    const math::Vec2 center = getBounds(*gui, "page").getCenter();
    const float top = getBounds(*gui, "content").y;
    touch(platform::Event::Type::TouchBegan, 1, center);
    frames();
    for (int step = 1; step <= 6; ++step) {
        touch(platform::Event::Type::TouchMoved, 1, center + math::Vec2{0.0F, 30.0F * static_cast<float>(step)});
        frames();
    }
    const float stretched = getBounds(*gui, "content").y - top;
    EXPECT_GT(stretched, 20.0F);
    EXPECT_LT(stretched, 150.0F);
    touch(platform::Event::Type::TouchEnded, 1, center + math::Vec2{0.0F, 180.0F});
    frames(60);
    EXPECT_EQ(getBounds(*gui, "content").y, top);
}

// A settings row placed at the width it measures keeps its label column whole and gives its control the width it asked for after the gap.
TEST_F(LayoutTest, PlacesASettingsRowAtTheWidthItMeasures) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "align": "start", "children": [
        {"kind": "settingsRow", "id": "volume", "label": "Volume", "children": [{"kind": "slider", "id": "level", "value": 0.5, "width": 600}]},
        {"kind": "settingsRow", "id": "music", "label": "Music", "children": [{"kind": "button", "text": "On"}]}
    ]})");
    // clang-format on
    frames(2);
    EXPECT_EQ(getBounds(*gui, "level").width, 600.0F);
    EXPECT_GE(getBounds(*gui, "music").width, getUi().getTheme().getMetric(Theme::Metric::SettingsLabelWidth) * 2.0F);
}

// An empty state with a message alone is as tall as its message, without the line of an empty title.
TEST_F(LayoutTest, LeavesTheEmptyPartsOfAnEmptyStateOut) {
    auto gui = mount(R"({"kind": "column", "align": "start", "children": [{"kind": "emptyState", "id": "alone", "message": "Nothing here"}, {"kind": "label", "id": "same", "text": "Nothing here"}]})");
    frames(2);
    EXPECT_EQ(getBounds(*gui, "alone").height, getBounds(*gui, "same").height);
}

// The title and the message of an empty state wrap at the reading width of the theme, centered, however wide the empty state is.
TEST_F(LayoutTest, WrapsTheTextOfAnEmptyStateAtTheReadingWidth) {
    const std::string message = "A message long enough to run across the whole width of a wide screen if nothing held it back to a comfortable width for reading it";
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "emptyState", "id": "empty", "title": "Nothing yet", "message": ")" + message + R"("}]})");
    frames(2);
    const float reading = getUi().getTheme().getMetric(Theme::Metric::ReadingWidth);
    const math::Rect bounds = getBounds(*gui, "empty");
    EXPECT_EQ(bounds.width, 1920.0F);
    ui::Backend& backend = getUi().getBackend();
    bool found = false;
    for (const ui::Backend::Text& text : backend.getTexts()) {
        if (backend.getContent(text) == message) {
            found = true;
            EXPECT_LE(text.style.maxWidth, reading);
            EXPECT_GE(text.origin.x, bounds.getCenter().x - reading * 0.5F - 1.0F);
        }
    }
    EXPECT_TRUE(found);
}

// The sliders of a column that show their values keep tracks of one length whatever their ends, since each keeps at least the value width of the theme for its value.
TEST_F(LayoutTest, LinesUpTheTracksOfSlidersThatShowTheirValues) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "column", "width": 600, "alignItems": "stretch", "children": [
        {"kind": "slider", "value": 0.2, "showValue": true},
        {"kind": "slider", "value": 30, "min": 0, "max": 300, "decimals": 0, "showValue": true},
        {"kind": "slider", "value": 3, "min": 1, "max": 9, "decimals": 0, "showValue": true}
    ]}]})");
    // clang-format on
    frames(2);
    const Theme& theme = getUi().getTheme();
    std::vector<float> tracks;
    for (const graphics2d::Shape& shape : getUi().getBackend().getShapes()) {
        if (shape.color == theme.getColor(Theme::Color::BorderStrong) && shape.bounds.height == theme.getMetric(Theme::Metric::SliderTrackHeight)) {
            tracks.push_back(shape.bounds.width);
        }
    }
    ASSERT_EQ(tracks.size(), 3U);
    EXPECT_EQ(tracks[0], tracks[1]);
    EXPECT_EQ(tracks[1], tracks[2]);
}

// The label of a progress bar draws in the ink of its tone over the fill and in the text color over the empty track, each cut at the edge of the fill, so it reads on both.
TEST_F(LayoutTest, DrawsTheLabelOfAProgressBarInTwoColorsAtTheEdgeOfItsFill) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "progress", "id": "bar", "width": 600, "value": 0.5, "tone": "success", "text": "Health 50"}]})");
    frames(2);
    const Theme& theme = getUi().getTheme();
    Backend& backend = getUi().getBackend();
    std::vector<math::Color> colors;
    for (const Backend::Text& text : backend.getTexts()) {
        if (backend.getContent(text) == "Health 50") {
            colors.push_back(text.style.color);
        }
    }
    ASSERT_EQ(colors.size(), 2U);
    EXPECT_EQ(colors[0], theme.getColor(Theme::Color::OnSuccess));
    EXPECT_EQ(colors[1], theme.getColor(Theme::Color::Text));
    getUi().getBackend().makeCurrent();
    const math::Rect bar = getBounds(*gui, "bar");
    EXPECT_TRUE(hasClip(math::Rect::fromMinMax(bar.getMin(), {bar.getCenter().x, bar.getBottom()})));
    EXPECT_TRUE(hasClip(math::Rect::fromMinMax({bar.getCenter().x, bar.y}, bar.getMax())));
}

// A slider shows a value that rounds to zero without a sign, and a number field draws its minus and plus as strokes of one weight rather than the hyphen of a font.
TEST_F(LayoutTest, ShowsNumbersWithoutANegativeZeroAndDrawsTheSignsOfNumberFields) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "slider", "width": 400, "value": -0.001, "min": -1, "max": 1, "showValue": true},
        {"kind": "numberField", "id": "count", "value": 3}
    ]})");
    // clang-format on
    frames(2);
    Backend& backend = getUi().getBackend();
    std::vector<std::string> texts;
    for (const Backend::Text& text : backend.getTexts()) {
        texts.emplace_back(backend.getContent(text));
    }
    EXPECT_NE(std::ranges::find(texts, "0.00"), texts.end());
    EXPECT_EQ(std::ranges::find(texts, "-0.00"), texts.end());
    EXPECT_EQ(std::ranges::find(texts, "-"), texts.end());
    EXPECT_EQ(std::ranges::find(texts, "+"), texts.end());

    const math::Rect field = getBounds(*gui, "count");
    const math::Color ink = getUi().getTheme().getColor(Theme::Color::Text);
    int strokes = 0;
    for (const graphics2d::Shape& shape : backend.getShapes()) {
        strokes += shape.color == ink && field.contains(shape.bounds.getCenter()) ? 1 : 0;
    }
    EXPECT_EQ(strokes, 3);
}

} // namespace haylen::ui
