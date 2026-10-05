#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <span>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/ui/Backend.hpp"
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
