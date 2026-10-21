#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/SafeAreaSimulation.hpp"
#include "haylen/ui/Theme.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

class AnchorTest : public ::testing::Test, public test::UiFixture {
  protected:
    AnchorTest() {
        getFixture().host().setSafeAreaInsets({.left = 100.0F, .top = 50.0F, .right = 80.0F, .bottom = 40.0F});
        frames();
    }
};

} // namespace

TEST_F(AnchorTest, AnchorsNodesToTheSafeAreaOrTheScreenOutsideTheLayout) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "label", "id": "title", "text": "Title"},
        {"kind": "button", "id": "pause", "text": "Pause", "width": 120, "height": 60, "anchor": "topRight", "margin": 10},
        {"kind": "button", "id": "map", "text": "Map", "width": 200, "height": 80, "anchor": "bottomLeft", "anchorTo": "screen"},
        {"kind": "button", "id": "middle", "text": "Middle", "width": 300, "height": 100, "anchor": "center"},
        {"kind": "panel", "id": "bar", "height": 90, "anchor": "stretchBottom", "margin": [0, 20]}
    ]})", Placement::Safe);
    // clang-format on
    EXPECT_EQ(getBounds(*gui, "title").getMin(), math::Vec2(100.0F, 50.0F));
    EXPECT_EQ(getBounds(*gui, "pause"), (math::Rect{1920.0F - 80.0F - 10.0F - 120.0F, 60.0F, 120.0F, 60.0F}));
    EXPECT_EQ(getBounds(*gui, "map"), (math::Rect{0.0F, 1000.0F, 200.0F, 80.0F}));
    EXPECT_EQ(getBounds(*gui, "middle"), (math::Rect{100.0F + (1740.0F - 300.0F) / 2.0F, 50.0F + (990.0F - 100.0F) / 2.0F, 300.0F, 100.0F}));
    EXPECT_EQ(getBounds(*gui, "bar"), (math::Rect{120.0F, 1040.0F - 90.0F, 1700.0F, 90.0F}));

    // A new safe area moves every anchored node with it, and the anchor `none` puts a node back in the layout, where its margin keeps it away from the node above.
    getFixture().host().setSafeAreaInsets({.left = 0.0F, .top = 0.0F, .right = 0.0F, .bottom = 0.0F});
    frames(2);
    EXPECT_EQ(getBounds(*gui, "pause").getMin(), math::Vec2(1920.0F - 10.0F - 120.0F, 10.0F));
    gui->set("pause", {{"anchor", "none"}});
    frames();
    EXPECT_EQ(getBounds(*gui, "pause").y, getBounds(*gui, "title").getBottom() + getUi().getTheme().getMetric(Theme::Metric::ItemSpacing) + 10.0F);
    EXPECT_THROW(gui->set("pause", {{"anchor", "corner"}}), std::invalid_argument);
    EXPECT_THROW(gui->set("pause", {{"anchorTo", "window"}}), std::invalid_argument);
}

// A node anchored to its parent sits against the bounds of its parent, such as touch controls in the corners of a play area that moves with the layout.
TEST_F(AnchorTest, AnchorsNodesToTheirParent) {
    // clang-format off
    auto gui = mount(R"({"kind": "row", "padding": 0, "gap": 0, "children": [
        {"kind": "spacer", "width": 300},
        {"kind": "stack", "id": "area", "grow": 1, "height": 600, "align": "start", "children": [
            {"kind": "playArea", "align": "stretch"},
            {"kind": "button", "id": "stick", "text": "Stick", "width": 200, "height": 200, "anchor": "bottomLeft", "anchorTo": "parent", "margin": 24},
            {"kind": "button", "id": "jump", "text": "Jump", "width": 120, "height": 120, "anchor": "bottomRight", "anchorTo": "parent", "margin": 24}
        ]}
    ]})", Placement::Screen);
    // clang-format on
    const math::Rect area = getBounds(*gui, "area");
    EXPECT_EQ(area, (math::Rect{300.0F, 0.0F, 1620.0F, 600.0F}));
    EXPECT_EQ(getBounds(*gui, "stick"), (math::Rect{324.0F, 600.0F - 24.0F - 200.0F, 200.0F, 200.0F}));
    EXPECT_EQ(getBounds(*gui, "jump"), (math::Rect{1920.0F - 24.0F - 120.0F, 600.0F - 24.0F - 120.0F, 120.0F, 120.0F}));

    gui->set("area", {{"height", 400}});
    frames();
    EXPECT_EQ(getBounds(*gui, "stick").getBottom(), 400.0F - 24.0F);
    EXPECT_THROW(gui->set("jump", {{"anchorTo", "window"}}), std::invalid_argument);
}

TEST_F(AnchorTest, AnchorsTheRootOfAGui) {
    auto gui = mount(R"({"kind": "panel", "id": "hud", "width": 400, "height": 100, "anchor": "top", "anchorTo": "screen", "margin": [16, 0]})", Placement::Safe);
    EXPECT_EQ(getBounds(*gui, "hud"), (math::Rect{760.0F, 16.0F, 400.0F, 100.0F}));

    auto stretched = mount(R"({"kind": "panel", "id": "side", "width": 300, "anchor": "stretchLeft"})", Placement::Screen);
    EXPECT_EQ(getBounds(*stretched, "side"), (math::Rect{100.0F, 50.0F, 300.0F, 990.0F}));
}

TEST_F(AnchorTest, MovesAnchoredNodesOutOfTheEdgesThatNativeViewsReserve) {
    auto gui = mount(R"({"kind": "button", "id": "buy", "text": "Buy", "width": 200, "height": 80, "anchor": "bottom"})", Placement::Safe);
    EXPECT_EQ(getBounds(*gui, "buy").getBottom(), 1040.0F);

    // A banner reserves the bottom edge, and the node anchored to the safe area moves above it until the banner goes.
    getFixture().host().getNativeViews().reserveInsets("banner", {.bottom = 150.0F});
    frames(2);
    EXPECT_EQ(getBounds(*gui, "buy").getBottom(), 930.0F);
    getFixture().host().getNativeViews().releaseInsets("banner");
    frames(2);
    EXPECT_EQ(getBounds(*gui, "buy").getBottom(), 1040.0F);
}

TEST_F(AnchorTest, SimulatesTheSafeAreaOfDevices) {
    const platform::SafeAreaSimulation island = platform::SafeAreaSimulation::fromJson("iphoneDynamicIsland");
    const math::Insets landscape = island.getInsets({1920.0F, 1080.0F}, 2.0F);
    EXPECT_NEAR(landscape.left, 59.0F * 1920.0F / 852.0F, 0.01F);
    EXPECT_NEAR(landscape.bottom, 21.0F * 1080.0F / 393.0F, 0.01F);
    EXPECT_EQ(landscape.top, 0.0F);
    const math::Insets portrait = island.getInsets({1080.0F, 1920.0F}, 2.0F);
    EXPECT_NEAR(portrait.top, 59.0F * 1920.0F / 852.0F, 0.01F);
    EXPECT_EQ(portrait.left, 0.0F);
    EXPECT_EQ(island.toJson(), "iphoneDynamicIsland");

    const platform::SafeAreaSimulation points = platform::SafeAreaSimulation::fromJson(core::Json::array({10, 20}));
    EXPECT_EQ(points.getInsets({800.0F, 600.0F}, 2.0F), (math::Insets{.left = 40.0F, .top = 20.0F, .right = 40.0F, .bottom = 20.0F}));
    EXPECT_EQ(points.toJson(), core::Json::array({10.0F, 20.0F, 10.0F, 20.0F}));
    EXPECT_THROW((void)platform::SafeAreaSimulation::fromJson("phone"), std::invalid_argument);
    EXPECT_THROW((void)platform::SafeAreaSimulation::fromJson(core::Json::array({1, 2, 3})), std::invalid_argument);
    EXPECT_THROW((void)platform::SafeAreaSimulation::fromJson(-4), std::invalid_argument);

    // The simulation replaces the safe area of the device until it is cleared.
    getEngine().setSafeAreaSimulation(platform::SafeAreaSimulation::fromJson("television"));
    frames();
    EXPECT_EQ(getEngine().getViewport().getSafeRect(), (math::Rect{80.0F, 60.0F, 1760.0F, 960.0F}));
    getEngine().setSafeAreaSimulation(std::nullopt);
    frames();
    EXPECT_EQ(getEngine().getViewport().getSafeRect(), (math::Rect{100.0F, 50.0F, 1740.0F, 990.0F}));
}

// The overlay of the safe area writes its insets in a band outside the safe area, where they cover no interface.
TEST_F(AnchorTest, WritesTheInsetsOfTheSafeAreaOutsideIt) {
    auto gui = mount(R"({"kind": "button", "id": "back", "text": "Back", "anchor": "topLeft"})", Placement::Safe);
    getUi().setSafeAreaVisible(true);
    frames(2);
    getUi().getBackend().makeCurrent();
    const math::Rect safe = getEngine().getViewport().getSafeRect().translated(-getEngine().getViewport().getVisibleRect().getMin());
    const ImU32 color = ImGui::ColorConvertFloat4ToU32({getUi().getTheme().getColor(Theme::Color::Warning).r, getUi().getTheme().getColor(Theme::Color::Warning).g, getUi().getTheme().getColor(Theme::Color::Warning).b, 1.0F});
    const ImVec2 white = ImGui::GetDrawListSharedData()->TexUvWhitePixel;
    int glyphs = 0;
    for (const ImDrawList* list : ImGui::GetDrawData()->CmdLists) {
        for (const ImDrawVert& vertex : list->VtxBuffer) {
            if (vertex.col != color || (vertex.uv.x == white.x && vertex.uv.y == white.y)) {
                continue;
            }
            ++glyphs;
            EXPECT_FALSE(safe.inset({4.0F, 4.0F, 4.0F, 4.0F}).contains(math::Vec2{vertex.pos.x, vertex.pos.y}));
        }
    }
    EXPECT_GT(glyphs, 0);
    EXPECT_FALSE(getBounds(*gui, "back").isEmpty());
}

TEST(SafeAreaConfigTest, ReadsTheSimulationFromTheAppFile) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({"debug": {"safeArea": "androidGestureBar", "showSafeArea": true}})"));
    ASSERT_TRUE(config.debug.safeArea.has_value());
    EXPECT_EQ(config.debug.safeArea->toJson(), "androidGestureBar");
    EXPECT_TRUE(config.debug.showSafeArea);
    EXPECT_EQ(core::AppConfig::fromJson(config.toJson()).toJson(), config.toJson());
    EXPECT_FALSE(core::AppConfig::fromJson(core::Json::object()).debug.safeArea.has_value());
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"debug": {"safeArea": "watch"}})")), std::invalid_argument);

    test::EngineFixture fixture({{"app.json", R"({"name": "Notch", "debug": {"safeArea": [0, 40], "showSafeArea": true}})"}});
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getViewport().getSafeRect(), (math::Rect{40.0F, 0.0F, 1840.0F, 1080.0F}));
    EXPECT_EQ(fixture.lua("local ui = require('haylen.ui') return tostring(ui.safeAreaVisible())"), "true");
    EXPECT_EQ(fixture.lua("local viewport = require('haylen.viewport') return table.concat(viewport.safeAreaSimulation(), ',')"), "0.0,40.0,0.0,40.0");
}

} // namespace haylen::ui
