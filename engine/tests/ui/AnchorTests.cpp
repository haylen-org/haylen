#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/SafeAreaSimulation.hpp"
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
    auto document = mount(R"({"kind": "column", "children": [
        {"kind": "label", "id": "title", "text": "Title"},
        {"kind": "button", "id": "pause", "text": "Pause", "width": 120, "height": 60, "anchor": "topRight", "margin": 10},
        {"kind": "button", "id": "map", "text": "Map", "width": 200, "height": 80, "anchor": "bottomLeft", "anchorTo": "screen"},
        {"kind": "button", "id": "middle", "text": "Middle", "width": 300, "height": 100, "anchor": "center"},
        {"kind": "panel", "id": "bar", "height": 90, "anchor": "stretchBottom", "margin": [0, 20]}
    ]})", Placement::Safe);
    // clang-format on
    EXPECT_EQ(getBounds(*document, "title").getMin(), math::Vec2(100.0F, 50.0F));
    EXPECT_EQ(getBounds(*document, "pause"), (math::Rect{1920.0F - 80.0F - 10.0F - 120.0F, 60.0F, 120.0F, 60.0F}));
    EXPECT_EQ(getBounds(*document, "map"), (math::Rect{0.0F, 1000.0F, 200.0F, 80.0F}));
    EXPECT_EQ(getBounds(*document, "middle"), (math::Rect{100.0F + (1740.0F - 300.0F) / 2.0F, 50.0F + (990.0F - 100.0F) / 2.0F, 300.0F, 100.0F}));
    EXPECT_EQ(getBounds(*document, "bar"), (math::Rect{120.0F, 1040.0F - 90.0F, 1700.0F, 90.0F}));

    // A new safe area moves every anchored node with it, and anchor none puts a node back in the layout.
    getFixture().host().setSafeAreaInsets({.left = 0.0F, .top = 0.0F, .right = 0.0F, .bottom = 0.0F});
    frames(2);
    EXPECT_EQ(getBounds(*document, "pause").getMin(), math::Vec2(1920.0F - 10.0F - 120.0F, 10.0F));
    document->set("pause", {{"anchor", "none"}});
    frames();
    EXPECT_EQ(getBounds(*document, "pause").y, getBounds(*document, "title").getBottom() + getUi().getTheme().getMetric(Theme::Metric::ItemSpacing));
    EXPECT_THROW(document->set("pause", {{"anchor", "corner"}}), std::invalid_argument);
    EXPECT_THROW(document->set("pause", {{"anchorTo", "window"}}), std::invalid_argument);
}

TEST_F(AnchorTest, AnchorsTheRootOfADocument) {
    auto document = mount(R"({"kind": "panel", "id": "hud", "width": 400, "height": 100, "anchor": "top", "anchorTo": "screen", "margin": [16, 0]})", Placement::Safe);
    EXPECT_EQ(getBounds(*document, "hud"), (math::Rect{760.0F, 16.0F, 400.0F, 100.0F}));

    auto stretched = mount(R"({"kind": "panel", "id": "side", "width": 300, "anchor": "stretchLeft"})", Placement::Screen);
    EXPECT_EQ(getBounds(*stretched, "side"), (math::Rect{100.0F, 50.0F, 300.0F, 990.0F}));
}

TEST_F(AnchorTest, MovesAnchoredNodesOutOfTheEdgesThatNativeViewsReserve) {
    auto document = mount(R"({"kind": "button", "id": "buy", "text": "Buy", "width": 200, "height": 80, "anchor": "bottom"})", Placement::Safe);
    EXPECT_EQ(getBounds(*document, "buy").getBottom(), 1040.0F);

    // A banner reserves the bottom edge, and the node anchored to the safe area moves above it until the banner goes.
    getFixture().host().getNativeViews().reserveInsets("banner", {.bottom = 150.0F});
    frames(2);
    EXPECT_EQ(getBounds(*document, "buy").getBottom(), 930.0F);
    getFixture().host().getNativeViews().releaseInsets("banner");
    frames(2);
    EXPECT_EQ(getBounds(*document, "buy").getBottom(), 1040.0F);
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
