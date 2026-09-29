#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/math/Polygon.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Monitor.hpp"
#include "haylen/platform/Window.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

TEST(WindowLuaTest, ChangesTheDesktopOptions) {
    test::EngineFixture fixture;
    fixture.runLua("window = require('haylen.window')");
    EXPECT_EQ(fixture.lua("return tostring(window.transparent()) .. ' ' .. tostring(window.decorated()) .. ' ' .. tostring(window.alwaysOnTop()) .. ' ' .. tostring(window.showInTaskbar()) .. ' ' .. tostring(window.focusable())"), "false true false true true");

    fixture.runLua("window.setDecorated(false) window.setAlwaysOnTop(true) window.setShowInTaskbar(false) window.setFocusable(false)");
    EXPECT_FALSE(fixture.host().isDecorated());
    EXPECT_TRUE(fixture.host().isAlwaysOnTop());
    EXPECT_FALSE(fixture.host().isShownInTaskbar());
    EXPECT_FALSE(fixture.host().isFocusable());
    EXPECT_EQ(fixture.lua("return tostring(window.decorated()) .. ' ' .. tostring(window.alwaysOnTop()) .. ' ' .. tostring(window.showInTaskbar()) .. ' ' .. tostring(window.focusable())"), "false true false false");

    EXPECT_EQ(fixture.lua("return window.canBeTransparent()"), "false");
    EXPECT_NE(fixture.lua("window.setTransparent(true)").find("window opened opaque"), std::string::npos);
    EXPECT_NE(fixture.lua("window.setDecorated(1)").find("error: "), std::string::npos);
}

TEST(WindowLuaTest, TurnsTransparencyOffAndOn) {
    test::EngineFixture fixture({{"app.json", R"({"window": {"transparent": true}})"}});
    fixture.runLua("window = require('haylen.window')");
    EXPECT_EQ(fixture.lua("return tostring(window.canBeTransparent()) .. ' ' .. tostring(window.transparent())"), "true true");
    fixture.runLua("window.setTransparent(false)");
    EXPECT_FALSE(fixture.host().isTransparent());
    EXPECT_EQ(fixture.lua("return window.transparent()"), "false");
    fixture.runLua("window.setTransparent(true)");
    EXPECT_TRUE(fixture.host().isTransparent());
}

TEST(WindowLuaTest, MovesAndPlacesTheWindow) {
    test::EngineFixture fixture;
    fixture.runLua("window = require('haylen.window')");
    EXPECT_EQ(fixture.lua("return tostring(window.frame())"), "Rect(0.0, 0.0, 1920.0, 1080.0)");

    fixture.runLua("window.setFrame(40, 60, 800, 200)");
    EXPECT_EQ(fixture.host().getFrame(), (math::Rect{40.0F, 60.0F, 800.0F, 200.0F}));
    EXPECT_NE(fixture.lua("window.setFrame(0, 0, 0, 200)").find("positive width"), std::string::npos);
    EXPECT_NE(fixture.lua("window.setFrame(0, 0, 800, -1)").find("positive height"), std::string::npos);

    // The headless desktop leaves a taskbar of 40 points at the bottom of its monitor.
    fixture.runLua("window.place({anchor = 'bottom', offset = {0, -8}})");
    EXPECT_EQ(fixture.host().getFrame(), (math::Rect{560.0F, 832.0F, 800.0F, 200.0F}));
    fixture.runLua("window.place({anchor = 'bottom', area = 'full', fill = 'width'})");
    EXPECT_EQ(fixture.host().getFrame(), (math::Rect{0.0F, 880.0F, 1920.0F, 200.0F}));
    fixture.runLua("window.place({x = 10, y = 20})");
    EXPECT_EQ(fixture.host().getFrame(), (math::Rect{10.0F, 20.0F, 1920.0F, 200.0F}));
    fixture.runLua("window.place('center')");
    EXPECT_EQ(fixture.host().getFrame(), (math::Rect{0.0F, 420.0F, 1920.0F, 200.0F}));
    EXPECT_NE(fixture.lua("window.place({anchor = 'middle'})").find("unknown anchor"), std::string::npos);
}

TEST(WindowLuaTest, ListsTheMonitors) {
    test::EngineFixture fixture;
    fixture.host().setMonitors({
        {.name = "main", .bounds = {0.0F, 0.0F, 1920.0F, 1080.0F}, .workArea = {0.0F, 25.0F, 1920.0F, 1055.0F}, .scale = 2.0F, .primary = true},
        {.name = "side", .bounds = {1920.0F, 0.0F, 1280.0F, 1024.0F}, .workArea = {1920.0F, 0.0F, 1280.0F, 1024.0F}, .scale = 1.0F, .primary = false},
    });
    fixture.runLua("window = require('haylen.window')");
    EXPECT_EQ(fixture.lua("local all = window.monitors() return #all .. ' ' .. all[1].name .. ' ' .. tostring(all[1].workArea) .. ' ' .. all[1].scale .. ' ' .. tostring(all[1].primary) .. ' ' .. all[2].name .. ' ' .. tostring(all[2].bounds) .. ' ' .. tostring(all[2].primary)"), "2 main Rect(0.0, 25.0, 1920.0, 1055.0) 2.0 true side Rect(1920.0, 0.0, 1280.0, 1024.0) false");

    fixture.host().setFrame({2200.0F, 100.0F, 640.0F, 480.0F});
    EXPECT_EQ(fixture.lua("return window.currentMonitor().name"), "side");
    fixture.runLua("window.place({anchor = 'topLeft', monitor = 2})");
    EXPECT_EQ(fixture.host().getFrame(), (math::Rect{1920.0F, 0.0F, 640.0F, 480.0F}));
}

TEST(WindowLuaTest, LetsClicksThroughOutsideTheRegions) {
    test::EngineFixture fixture;
    fixture.host().resize({960.0F, 540.0F});
    fixture.frames(1);
    fixture.runLua("window = require('haylen.window')");
    EXPECT_EQ(fixture.lua("return window.mousePassthrough()"), "off");

    fixture.runLua("window.setMousePassthrough(true)");
    EXPECT_EQ(fixture.host().getMousePassthrough(), Window::Passthrough::Whole);
    EXPECT_EQ(fixture.lua("return window.mousePassthrough()"), "whole");

    // Regions in design units follow the viewport, which shows the 1920 by 1080 design at half size here.
    fixture.runLua("window.setMousePassthrough({{x = 100, y = 200, width = 300, height = 100}, {{0, 0}, {400, 0}, {0, 400}}, require('haylen.math').rect(1000, 0, 20, 40)})");
    EXPECT_EQ(fixture.host().getMousePassthrough(), Window::Passthrough::Regions);
    EXPECT_EQ(fixture.lua("return window.mousePassthrough()"), "regions");
    const std::vector<math::Polygon::Outline> expected{
        {{50.0F, 100.0F}, {200.0F, 100.0F}, {200.0F, 150.0F}, {50.0F, 150.0F}},
        {{0.0F, 0.0F}, {200.0F, 0.0F}, {0.0F, 200.0F}},
        {{500.0F, 0.0F}, {510.0F, 0.0F}, {510.0F, 20.0F}, {500.0F, 20.0F}},
    };
    EXPECT_EQ(fixture.host().getPassthroughRegions(), expected);

    fixture.runLua("window.setMousePassthrough({{8, 16, 32, 64}}, 'pixels')");
    EXPECT_EQ(fixture.host().getPassthroughRegions(), (std::vector<math::Polygon::Outline>{{{8.0F, 16.0F}, {40.0F, 16.0F}, {40.0F, 80.0F}, {8.0F, 80.0F}}}));
    fixture.runLua("window.setMousePassthrough({})");
    EXPECT_EQ(fixture.host().getMousePassthrough(), Window::Passthrough::Regions);
    EXPECT_TRUE(fixture.host().getPassthroughRegions().empty());
    fixture.runLua("window.setMousePassthrough(false)");
    EXPECT_EQ(fixture.host().getMousePassthrough(), Window::Passthrough::Off);

    EXPECT_NE(fixture.lua("window.setMousePassthrough('everywhere')").find("boolean or table of regions expected"), std::string::npos);
    EXPECT_NE(fixture.lua("window.setMousePassthrough({{{0, 0}, {1, 1}}})").find("at least three points"), std::string::npos);
    EXPECT_NE(fixture.lua("window.setMousePassthrough({42})").find("rectangle or a polygon"), std::string::npos);
    EXPECT_NE(fixture.lua("window.setMousePassthrough({{0, 0, 10, 10}}, 'points')").find("'design' or 'pixels' expected"), std::string::npos);
}

TEST(WindowLuaTest, StartsDragsAndHearsTheDesktop) {
    test::EngineFixture fixture;
    fixture.runLua("window = require('haylen.window') window.startDrag() window.startDrag()");
    EXPECT_EQ(fixture.host().getDragCount(), 2);

    // Scenes hear the moves of the window and the changes of the monitors among their events.
    fixture.runLua("heard = {} require('haylen.scene').push({event = function(self, event) heard[#heard + 1] = event.type end})");
    fixture.frames(1);
    fixture.engine().handleEvent({.type = Event::Type::WindowMoved});
    fixture.engine().handleEvent({.type = Event::Type::MonitorsChanged});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ' ')"), "window_moved monitors_changed");
}

} // namespace haylen::platform
