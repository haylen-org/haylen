#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Monitor.hpp"
#include "haylen/platform/WindowPlacement.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "sokol_gfx.h"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::platform {

namespace {

class MonitorLayoutTest : public ::testing::Test {
  protected:
    // A primary monitor with a taskbar at its bottom, and a denser one to its left whose top sits lower.
    [[nodiscard]] static std::vector<Monitor> twoMonitors() {
        return {
            {.name = "main", .bounds = {0.0F, 0.0F, 1920.0F, 1080.0F}, .workArea = {0.0F, 0.0F, 1920.0F, 1040.0F}, .scale = 1.0F, .primary = true},
            {.name = "side", .bounds = {-1440.0F, 200.0F, 1440.0F, 900.0F}, .workArea = {-1440.0F, 225.0F, 1440.0F, 875.0F}, .scale = 2.0F, .primary = false},
        };
    }

    [[nodiscard]] static math::Rect place(const char* position, const std::vector<Monitor>& monitors = twoMonitors(), math::Vec2 size = {400.0F, 100.0F}) {
        return WindowPlacement::fromJson(core::Json::parse(position)).resolve(monitors, size);
    }
};

class WindowConfigTest : public MonitorLayoutTest {};

class WindowPlacementTest : public MonitorLayoutTest {};

class WindowTest : public MonitorLayoutTest {};

// Records what the renderer asks of sokol_gfx while a frame reaches the swapchain: the alpha it clears to and the channels its pipelines write.
class SwapchainTrace final {
  public:
    SwapchainTrace() {
        sg_trace_hooks hooks{};
        hooks.user_data = this;
        hooks.make_pipeline = &onMakePipeline;
        hooks.begin_pass = &onBeginPass;
        hooks.apply_pipeline = &onApplyPipeline;
        hooks.end_pass = &onEndPass;
        previous = sg_install_trace_hooks(&hooks);
    }
    ~SwapchainTrace() {
        sg_install_trace_hooks(&previous);
    }
    SwapchainTrace(const SwapchainTrace&) = delete;
    SwapchainTrace& operator=(const SwapchainTrace&) = delete;

    void clear() {
        clearAlphas.clear();
        writtenMasks.clear();
    }

    std::vector<float> clearAlphas;
    std::vector<sg_color_mask> writtenMasks;

  private:
    static void onMakePipeline(const sg_pipeline_desc* desc, sg_pipeline result, void* data) {
        static_cast<SwapchainTrace*>(data)->masks[result.id] = desc->colors[0].write_mask;
    }
    static void onBeginPass(const sg_pass* pass, void* data) {
        auto& trace = *static_cast<SwapchainTrace*>(data);
        trace.swapchain = pass->attachments.colors[0].id == SG_INVALID_ID;
        if (trace.swapchain) {
            trace.clearAlphas.push_back(pass->action.colors[0].clear_value.a);
        }
    }
    static void onApplyPipeline(sg_pipeline pipeline, void* data) {
        auto& trace = *static_cast<SwapchainTrace*>(data);
        if (trace.swapchain) {
            trace.writtenMasks.push_back(trace.masks.at(pipeline.id));
        }
    }
    static void onEndPass(void* data) {
        static_cast<SwapchainTrace*>(data)->swapchain = false;
    }

    sg_trace_hooks previous{};
    std::map<std::uint32_t, sg_color_mask> masks;
    bool swapchain = false;
};

} // namespace

TEST_F(WindowConfigTest, ReadsTheDesktopOptionsOfAppJson) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({
        "window": {"decorated": false, "transparent": true, "alwaysOnTop": true, "showInTaskbar": false, "focusable": false, "mousePassthrough": true,
                   "position": {"anchor": "bottom", "area": "work", "monitor": "primary", "offset": [0, -8], "fill": "width"}}
    })"));
    EXPECT_FALSE(config.window.decorated);
    EXPECT_TRUE(config.window.transparent);
    EXPECT_TRUE(config.window.alwaysOnTop);
    EXPECT_FALSE(config.window.showInTaskbar);
    EXPECT_FALSE(config.window.focusable);
    EXPECT_TRUE(config.window.mousePassthrough);
    ASSERT_TRUE(config.window.position.has_value());
    EXPECT_EQ(config.window.position->resolve(twoMonitors(), {400.0F, 100.0F}), (math::Rect{0.0F, 932.0F, 1920.0F, 100.0F}));
    EXPECT_EQ(core::AppConfig::fromJson(config.toJson()).toJson(), config.toJson());

    // A transparent window clears to transparent unless clearColor says otherwise, and so does its splash screen.
    EXPECT_EQ(config.clearColor, math::Color::transparent());
    EXPECT_EQ(config.splash.background, math::Color::transparent());
    EXPECT_EQ(core::AppConfig::fromJson(core::Json::parse(R"({"window": {"transparent": true}, "clearColor": "#102030"})")).clearColor, math::Color::fromHex(0x102030FFU));

    const core::AppConfig defaults = core::AppConfig::fromJson(core::Json::object());
    EXPECT_TRUE(defaults.window.decorated);
    EXPECT_FALSE(defaults.window.transparent);
    EXPECT_FALSE(defaults.window.alwaysOnTop);
    EXPECT_TRUE(defaults.window.showInTaskbar);
    EXPECT_TRUE(defaults.window.focusable);
    EXPECT_FALSE(defaults.window.mousePassthrough);
    EXPECT_FALSE(defaults.window.position.has_value());
    EXPECT_FALSE(defaults.toJson().at("window").contains("position"));
}

TEST_F(WindowConfigTest, RejectsInvalidDesktopOptions) {
    const char* documents[] = {
        R"({"window": {"decorated": "no"}})", R"({"window": {"mousePassthrough": "regions"}})", R"({"window": {"position": "left"}})", R"({"window": {"position": 12}})", R"({"window": {"position": {"anchor": "middle"}}})", R"({"window": {"position": {"area": "screen"}}})", R"({"window": {"position": {"fill": "all"}}})", R"({"window": {"position": {"monitor": 0}}})", R"({"window": {"position": {"monitor": "left"}}})", R"({"window": {"position": {"offset": [1]}}})", R"({"window": {"position": {"offset": [1, "a"]}}})", R"({"window": {"position": {"x": 3}}})", R"({"window": {"position": {"x": 3, "y": 4, "anchor": "top"}}})", R"({"window": {"position": {"anchor": "top", "size": 3}}})",
    };
    for (const char* document : documents) {
        EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(document)), std::invalid_argument) << document;
    }
}

TEST_F(WindowPlacementTest, AnchorsWindowsToTheAreaOfAMonitor) {
    EXPECT_EQ(place(R"("center")"), (math::Rect{760.0F, 470.0F, 400.0F, 100.0F}));
    EXPECT_EQ(place(R"({"anchor": "bottom"})"), (math::Rect{760.0F, 940.0F, 400.0F, 100.0F}));
    EXPECT_EQ(place(R"({"anchor": "bottom", "area": "full"})"), (math::Rect{760.0F, 980.0F, 400.0F, 100.0F}));
    EXPECT_EQ(place(R"({"anchor": "top"})"), (math::Rect{760.0F, 0.0F, 400.0F, 100.0F}));
    EXPECT_EQ(place(R"({"anchor": "left"})"), (math::Rect{0.0F, 470.0F, 400.0F, 100.0F}));
    EXPECT_EQ(place(R"({"anchor": "right"})"), (math::Rect{1520.0F, 470.0F, 400.0F, 100.0F}));
    EXPECT_EQ(place(R"({"anchor": "topLeft", "offset": [16, 24]})"), (math::Rect{16.0F, 24.0F, 400.0F, 100.0F}));
    EXPECT_EQ(place(R"({"anchor": "topRight"})"), (math::Rect{1520.0F, 0.0F, 400.0F, 100.0F}));
    EXPECT_EQ(place(R"({"anchor": "bottomLeft"})"), (math::Rect{0.0F, 940.0F, 400.0F, 100.0F}));
    EXPECT_EQ(place(R"({"anchor": "bottomRight", "offset": [-10, -10]})"), (math::Rect{1510.0F, 930.0F, 400.0F, 100.0F}));

    // Monitors count from 1, and a monitor past the last one is the primary monitor.
    EXPECT_EQ(place(R"({"anchor": "bottom", "monitor": 2})"), (math::Rect{-920.0F, 1000.0F, 400.0F, 100.0F}));
    EXPECT_EQ(place(R"({"anchor": "top", "monitor": 1})"), (math::Rect{760.0F, 0.0F, 400.0F, 100.0F}));
    EXPECT_EQ(place(R"({"anchor": "top", "monitor": 3})"), (math::Rect{760.0F, 0.0F, 400.0F, 100.0F}));

    // Without a primary monitor, the first monitor takes its place.
    std::vector<Monitor> unflagged = twoMonitors();
    unflagged.front().primary = false;
    EXPECT_EQ(place(R"({"anchor": "topLeft"})", unflagged), (math::Rect{0.0F, 0.0F, 400.0F, 100.0F}));
}

TEST_F(WindowPlacementTest, FillsAreasAndPlacesPoints) {
    EXPECT_EQ(place(R"({"anchor": "bottom", "fill": "width"})"), (math::Rect{0.0F, 940.0F, 1920.0F, 100.0F}));
    EXPECT_EQ(place(R"({"anchor": "right", "fill": "height"})"), (math::Rect{1520.0F, 0.0F, 400.0F, 1040.0F}));
    EXPECT_EQ(place(R"({"fill": "both", "area": "full", "monitor": 2})"), (math::Rect{-1440.0F, 200.0F, 1440.0F, 900.0F}));
    EXPECT_EQ(place(R"({"x": -300.5, "y": 250})"), (math::Rect{-300.5F, 250.0F, 400.0F, 100.0F}));

    // Anchored windows land on whole points, so their edges stay sharp.
    EXPECT_EQ(place(R"("center")", twoMonitors(), {401.0F, 101.0F}), (math::Rect{760.0F, 470.0F, 401.0F, 101.0F}));

    for (const char* position : {R"("center")", R"({"x": 12, "y": 34})", R"({"anchor": "bottomRight", "area": "full", "monitor": 2, "offset": [-4, 6], "fill": "height"})"}) {
        const WindowPlacement placement = WindowPlacement::fromJson(core::Json::parse(position));
        EXPECT_EQ(WindowPlacement::fromJson(placement.toJson()), placement) << position;
    }
}

TEST_F(WindowTest, FindsTheMonitorThatHoldsMostOfTheWindow) {
    test::TemporaryDirectory directory;
    HeadlessHost host(directory.getPath());
    host.setMonitors(twoMonitors());

    host.setFrame({-1000.0F, 400.0F, 800.0F, 600.0F});
    EXPECT_EQ(host.getCurrentMonitor().name, "side");
    host.setFrame({-300.0F, 400.0F, 800.0F, 600.0F});
    EXPECT_EQ(host.getCurrentMonitor().name, "main");

    // A window off every monitor belongs to the primary one.
    host.setFrame({5000.0F, 5000.0F, 800.0F, 600.0F});
    EXPECT_EQ(host.getCurrentMonitor().name, "main");
}

TEST(WindowEngineTest, AppliesTheWindowOptionsOfAppJson) {
    test::EngineFixture fixture({{"app.json", R"({"window": {"decorated": false, "alwaysOnTop": true, "showInTaskbar": false, "focusable": false, "mousePassthrough": true, "resizable": false}})"}});
    const HeadlessHost& host = fixture.host();
    EXPECT_FALSE(host.isDecorated());
    EXPECT_TRUE(host.isAlwaysOnTop());
    EXPECT_FALSE(host.isShownInTaskbar());
    EXPECT_FALSE(host.isFocusable());
    EXPECT_FALSE(host.isResizable());
    EXPECT_EQ(host.getMousePassthrough(), Window::Passthrough::Whole);
    EXPECT_TRUE(host.getPassthroughRegions().empty());
}

TEST(WindowEngineTest, OpensAnOrdinaryWindowByDefault) {
    test::EngineFixture fixture;
    const HeadlessHost& host = fixture.host();
    EXPECT_TRUE(host.isDecorated());
    EXPECT_FALSE(host.isAlwaysOnTop());
    EXPECT_TRUE(host.isShownInTaskbar());
    EXPECT_TRUE(host.isFocusable());
    EXPECT_EQ(host.getMousePassthrough(), Window::Passthrough::Off);
}

TEST(WindowEngineTest, TurnsTransparencyOffAndOnAgain) {
    test::EngineFixture fixture({{"app.json", R"({"window": {"transparent": true}})"}});
    EXPECT_TRUE(fixture.host().canBeTransparent());
    EXPECT_TRUE(fixture.host().isTransparent());
    EXPECT_TRUE(fixture.host().getFrameTarget().transparent);

    fixture.host().setTransparent(false);
    EXPECT_FALSE(fixture.host().isTransparent());
    EXPECT_FALSE(fixture.host().getFrameTarget().transparent);
    fixture.host().setTransparent(true);
    EXPECT_TRUE(fixture.host().getFrameTarget().transparent);
}

TEST(WindowEngineTest, KeepsAWindowThatOpenedOpaqueOpaque) {
    test::EngineFixture fixture;
    EXPECT_FALSE(fixture.host().canBeTransparent());
    EXPECT_FALSE(fixture.host().isTransparent());
    EXPECT_THROW(fixture.host().setTransparent(true), std::logic_error);
    EXPECT_NO_THROW(fixture.host().setTransparent(false));
}

// Whatever the clear color and the blends, an opaque window clears to alpha 1 and draws no alpha, while a transparent one clears to the clear color and blends its alpha.
TEST(WindowEngineTest, KeepsTheAlphaOfOpaqueWindowsAtOne) {
    test::EngineFixture fixture({
        {"app.json", R"({"window": {"transparent": true}, "clearColor": "#00000000"})"},
        {"source/main.lua", "local graphics2d = require('haylen.graphics2d') local camera = graphics2d.newCamera() "
                            "require('haylen.scene').push({render = function() "
                            "graphics2d.beginWorld(camera, {ambientLight = '#FF808080', postProcess = {fade = '#40FFFFFF'}}) graphics2d.drawRect({0, 0, 100, 100}, '#80FF0000') "
                            "graphics2d.beginScreen() graphics2d.drawRect({0, 0, 50, 50}, '#40FFFFFF', {blend = 'additive'}) graphics2d.drawText(nil, 'Gold', 10, 10) end})"},
    });
    SwapchainTrace trace;
    fixture.frames(2);
    ASSERT_FALSE(trace.clearAlphas.empty());
    EXPECT_EQ(trace.clearAlphas.back(), 0.0F);
    ASSERT_FALSE(trace.writtenMasks.empty());
    for (const sg_color_mask mask : trace.writtenMasks) {
        EXPECT_EQ(mask, SG_COLORMASK_RGBA);
    }

    fixture.host().setTransparent(false);
    trace.clear();
    fixture.frames(2);
    ASSERT_FALSE(trace.clearAlphas.empty());
    EXPECT_EQ(trace.clearAlphas.back(), 1.0F);
    ASSERT_GE(trace.writtenMasks.size(), 3U);
    for (const sg_color_mask mask : trace.writtenMasks) {
        EXPECT_EQ(mask, SG_COLORMASK_RGB);
    }
}

TEST(WindowEngineTest, PublishesMovesAndMonitorChanges) {
    test::EngineFixture fixture;
    fixture.runLua("events = require('haylen.events') heard = {} "
                   "events.on('windowMoved', function(info) heard[#heard + 1] = 'moved ' .. info.x .. ' ' .. info.y end) "
                   "events.on('windowMonitorsChanged', function() heard[#heard + 1] = 'monitors' end)");

    // Platforms report every step of a move, and the app hears each new position once.
    fixture.host().setFrame({120.0F, 80.0F, 1920.0F, 1080.0F});
    fixture.engine().handleEvent({.type = Event::Type::WindowMoved});
    fixture.engine().handleEvent({.type = Event::Type::WindowMoved});
    fixture.host().setFrame({120.0F, 80.0F, 640.0F, 480.0F});
    fixture.engine().handleEvent({.type = Event::Type::WindowMoved});
    fixture.engine().handleEvent({.type = Event::Type::MonitorsChanged});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "moved 120.0 80.0, monitors");
}

} // namespace haylen::platform
