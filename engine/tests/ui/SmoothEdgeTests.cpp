#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <span>

#include <imgui.h>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Theme.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

class SmoothEdgeTest : public ::testing::Test, public test::UiFixture {
  protected:
    // The last shape of the frame that matches, or nothing.
    template <typename Match> [[nodiscard]] std::optional<graphics2d::Shape> findShape(Match&& match) {
        const std::span<const graphics2d::Shape> shapes = getUi().getBackend().getShapes();
        const auto found = std::find_if(shapes.rbegin(), shapes.rend(), match);
        return found != shapes.rend() ? std::optional(*found) : std::nullopt;
    }

    [[nodiscard]] float getMetric(Theme::Metric role) {
        return getUi().getTheme().getMetric(role);
    }

    [[nodiscard]] math::Color getColor(Theme::Color role) {
        return getUi().getTheme().getColor(role);
    }

    [[nodiscard]] static std::array<float, 4> round(float radius) {
        return {radius, radius, radius, radius};
    }
};

} // namespace

// The ring of a focused control that a transform scales stays inside the clip around it, which scales with it, where the edge of the screen cuts the ring.
TEST_F(SmoothEdgeTest, ClipsTheFocusRingOfAScaledControlAroundItsShape) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "button", "id": "corner", "text": "Corner", "autofocus": true}]})");
    gui->find("corner")->getTransform()->scale = {1.5F, 1.5F};
    key(input::Key::Down);
    frames(2);

    const math::Color focus = getColor(Theme::Color::Focus);
    const std::optional<graphics2d::Shape> ring = findShape([&focus](const graphics2d::Shape& shape) { return shape.borderColor == focus; });
    ASSERT_TRUE(ring.has_value());
    getUi().getBackend().makeCurrent();
    bool clippedAround = false;
    for (const ImDrawList* list : ImGui::GetDrawData()->CmdLists) {
        for (const ImDrawCmd& command : list->CmdBuffer) {
            const math::Rect clip = math::Rect::fromMinMax({command.ClipRect.x, command.ClipRect.y}, {command.ClipRect.z, command.ClipRect.w});
            clippedAround = clippedAround || (command.UserCallback != nullptr && clip.contains(ring->bounds));
        }
    }
    EXPECT_TRUE(clippedAround);
}

// A divider draws its line inside its bounds, on whole units.
TEST_F(SmoothEdgeTest, DrawsDividersInsideTheirBounds) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "label", "text": "Above"}, {"kind": "divider", "id": "rule"}, {"kind": "label", "text": "Below"}]})");
    frames(2);
    const math::Color border = getColor(Theme::Color::Border);
    const std::optional<graphics2d::Shape> line = findShape([&border](const graphics2d::Shape& shape) { return shape.color == border; });
    ASSERT_TRUE(line.has_value());
    const math::Rect bounds = getBounds(*gui, "rule");
    EXPECT_TRUE(bounds.contains(line->bounds)) << line->bounds.x << " " << line->bounds.y << " " << line->bounds.width << " " << line->bounds.height;
    EXPECT_EQ(line->bounds.height, getMetric(Theme::Metric::BorderWidth));
}

// The line of a splitter runs through the middle of the gap between its panes.
TEST_F(SmoothEdgeTest, CentersTheLineOfASplitterBetweenItsPanes) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "splitter", "height": 100, "children": [{"kind": "label", "id": "left", "text": "Left"}, {"kind": "label", "id": "right", "text": "Right"}]}]})");
    frames(2);
    const math::Color strong = getColor(Theme::Color::BorderStrong);
    const std::optional<graphics2d::Shape> line = findShape([&strong](const graphics2d::Shape& shape) { return shape.color == strong; });
    ASSERT_TRUE(line.has_value());
    const math::Rect left = getBounds(*gui, "left");
    EXPECT_FLOAT_EQ(line->bounds.getCenter().x, (left.getRight() + getBounds(*gui, "right").x) * 0.5F);
}

// The separators of a segmented control stand centered on the edges between its segments.
TEST_F(SmoothEdgeTest, CentersTheSeparatorsOfASegmentedControlOnTheirEdges) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "segmentedControl", "id": "view", "width": 600, "items": [{"id": "a", "text": "A"}, {"id": "b", "text": "B"}, {"id": "c", "text": "C"}], "selected": "a"}]})");
    frames(2);
    const math::Color accent = getColor(Theme::Color::Accent);
    const math::Color border = getColor(Theme::Color::Border);
    const std::optional<graphics2d::Shape> chosen = findShape([&accent](const graphics2d::Shape& shape) { return shape.color == accent; });
    const std::optional<graphics2d::Shape> separator = findShape([&border](const graphics2d::Shape& shape) { return shape.color == border; });
    ASSERT_TRUE(chosen.has_value() && separator.has_value());
    EXPECT_FLOAT_EQ(separator->bounds.getCenter().x, std::floor(chosen->bounds.x + chosen->bounds.width * 2.0F));
}

// A cooldown wider than its dial draws its backdrop as the circle of the dial, never as a pill across its whole width.
TEST_F(SmoothEdgeTest, DrawsTheBackdropOfACooldownAsItsDial) {
    auto gui = mount(R"({"kind": "column", "align": "start", "children": [{"kind": "circularProgress", "id": "cooldown", "variant": "cooldown", "value": 0.5, "size": 96, "width": 200}]})");
    frames(2);
    const math::Color backdrop = getColor(Theme::Color::AccentBackground);
    const std::optional<graphics2d::Shape> dial = findShape([&backdrop](const graphics2d::Shape& shape) { return shape.color == backdrop; });
    ASSERT_TRUE(dial.has_value());
    EXPECT_EQ(dial->bounds.width, dial->bounds.height);
    EXPECT_EQ(dial->bounds.getCenter(), getBounds(*gui, "cooldown").getCenter());
}

// Every curved control draws through shapes whose edges the shader covers exactly, so a frame of them uploads no vertices of ImGui at all.
TEST_F(SmoothEdgeTest, DrawsTheCurvesOfControlsAsShapes) {
    // clang-format off
    mount(R"({"kind": "column", "padding": 20, "gap": 12, "children": [
        {"kind": "button", "text": "Play"}, {"kind": "button", "text": "Go", "variant": "primary"},
        {"kind": "toggle", "text": "Sound", "checked": true}, {"kind": "checkbox", "text": "Hints", "checked": true},
        {"kind": "radioGroup", "items": [{"id": "easy", "text": "Easy"}, {"id": "hard", "text": "Hard"}], "selected": "easy"},
        {"kind": "slider", "value": 0.5}, {"kind": "progress", "value": 0.5}, {"kind": "circularProgress", "value": 0.4},
        {"kind": "circularProgress", "value": 0.4, "variant": "cooldown"}, {"kind": "busyIndicator"},
        {"kind": "badge", "text": "3"}, {"kind": "chip", "text": "Wood", "removable": true}, {"kind": "statusIndicator", "text": "Online"},
        {"kind": "avatar", "name": "Ana Souza"}, {"kind": "card", "children": [{"kind": "label", "text": "Card"}]}
    ]})");
    // clang-format on
    frames(2);
    EXPECT_EQ(getEngine().getRenderer2D().getStats().vertices, 0U);
    EXPECT_GE(getUi().getBackend().getShapes().size(), 25U);
}

// A surface fills and borders one shape, so its border follows its corners on the same edge, without a seam between the fill and the border.
TEST_F(SmoothEdgeTest, PaintsTheFillAndTheBorderOfASurfaceInOneShape) {
    const std::shared_ptr<Gui> gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "button", "id": "play", "text": "Play"}]})");
    frames(2);
    const math::Rect bounds = getBounds(*gui, "play");
    const std::optional<graphics2d::Shape> surface = findShape([&bounds](const graphics2d::Shape& shape) { return shape.bounds == bounds; });
    ASSERT_TRUE(surface.has_value());
    EXPECT_EQ(surface->radii, round(std::min(getMetric(Theme::Metric::ControlRadius), bounds.height * 0.5F)));
    EXPECT_EQ(surface->color, getColor(Theme::Color::Raised));
    EXPECT_EQ(surface->borderColor, getColor(Theme::Color::Border));
    EXPECT_FLOAT_EQ(surface->borderWidth, getMetric(Theme::Metric::BorderWidth));
}

// The focus ring is the border of a shape the gap away from the control, with corners concentric to the corners of the control.
TEST_F(SmoothEdgeTest, RoundsTheFocusRingAroundTheCornersOfItsControl) {
    const std::shared_ptr<Gui> gui = mount(R"({"kind": "column", "padding": 40, "children": [{"kind": "button", "id": "play", "text": "Play", "autofocus": true}]})");
    key(input::Key::Right);
    frames(1);
    const math::Color focus = getColor(Theme::Color::Focus);
    const std::optional<graphics2d::Shape> ring = findShape([focus](const graphics2d::Shape& shape) { return shape.borderColor == focus; });
    ASSERT_TRUE(ring.has_value());
    const float reach = getMetric(Theme::Metric::FocusGap) + getMetric(Theme::Metric::FocusWidth);
    EXPECT_EQ(ring->bounds, getBounds(*gui, "play").expanded(reach));
    EXPECT_EQ(ring->radii, round(getMetric(Theme::Metric::ControlRadius) + reach));
    EXPECT_FLOAT_EQ(ring->borderWidth, getMetric(Theme::Metric::FocusWidth));
    EXPECT_EQ(ring->color.a, 0.0F);
}

// A floating surface casts one shape whose edge fades over the size of the shadow, as dark as the shadow color along the edge of the surface.
TEST_F(SmoothEdgeTest, CastsOneSoftShadowUnderFloatingSurfaces) {
    mount(R"({"kind": "dialog", "open": true, "title": "Quit?", "message": "Progress is saved.", "buttons": [{"id": "no", "text": "Stay"}]})");
    frames(30);
    const float size = getMetric(Theme::Metric::ShadowSize);
    const std::optional<graphics2d::Shape> shadow = findShape([size](const graphics2d::Shape& shape) { return shape.softness == size; });
    ASSERT_TRUE(shadow.has_value());
    EXPECT_EQ(shadow->color, getColor(Theme::Color::Shadow));
    EXPECT_FLOAT_EQ(shadow->radii[0], getMetric(Theme::Metric::PanelRadius) + size * 0.5F);
}

} // namespace haylen::ui
