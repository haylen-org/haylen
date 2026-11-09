#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/platform/Display.hpp"
#include "haylen/platform/Displays.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Outputs.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

class DisplayLuaTest : public ::testing::Test {
  protected:
    // A primary display with a menu bar and a slower one to its right.
    [[nodiscard]] static std::vector<Display> twoDisplays() {
        return {
            {.id = "main-id", .name = "main", .bounds = {0.0F, 0.0F, 1920.0F, 1080.0F}, .workArea = {0.0F, 25.0F, 1920.0F, 1055.0F}, .scale = 2.0F, .refreshRate = 120.0F, .primary = true, .modes = {{.width = 3840, .height = 2160, .refreshRate = 120.0F, .scale = 2.0F}}},
            {.id = "side-id", .name = "side", .bounds = {1920.0F, 0.0F, 1280.0F, 1024.0F}, .workArea = {1920.0F, 0.0F, 1280.0F, 1024.0F}, .scale = 1.0F, .refreshRate = 60.0F, .modes = {{.width = 1280, .height = 1024, .refreshRate = 60.0F}, {.width = 1024, .height = 768, .refreshRate = 75.0F}}},
        };
    }

    // Opens an output of the Lua global `output` on the side display and shows it on a surface of the size.
    static void showOutput(test::EngineFixture& fixture, int width, int height) {
        fixture.host().setDisplays(twoDisplays());
        fixture.engine().handleEvent({.type = Event::Type::DisplaysChanged});
        fixture.runLua("display = require('haylen.display') events = require('haylen.events') heard = {} "
                       "for _, name in ipairs({'outputShown', 'outputHidden', 'outputResized', 'outputClosed', 'outputInput'}) do "
                       "events.on(name, function(info) heard[#heard + 1] = name .. ' ' .. (info.width or info.ending or info.type or '') end) end "
                       "output = display.open('side-id', {fullscreen = false, width = 800, height = 600, title = 'Board', clearColor = '#FF102030'})");
        report(fixture, width, height);
    }

    static void report(test::EngineFixture& fixture, int width, int height) {
        fixture.host().setOutputReport(1, {.surface = gpu::Surface{.width = width, .height = height}, .scale = 1.0F});
        fixture.engine().handleEvent({.type = Event::Type::OutputChanged, .output = 1});
    }
};

TEST_F(DisplayLuaTest, ListsTheDisplaysWithTheirModes) {
    test::EngineFixture fixture;
    fixture.host().setDisplays(twoDisplays());
    fixture.engine().handleEvent({.type = Event::Type::DisplaysChanged});
    fixture.runLua("display = require('haylen.display') window = require('haylen.window')");
    EXPECT_EQ(fixture.lua("local all = display.list() return #all .. ' ' .. all[1].id .. ' ' .. all[1].name .. ' ' .. tostring(all[1].workArea) .. ' ' .. all[1].scale .. ' ' .. all[1].refreshRate .. ' ' .. tostring(all[1].primary)"), "2 main-id main Rect(0.0, 25.0, 1920.0, 1055.0) 2.0 120.0 true");
    EXPECT_EQ(fixture.lua("local side = display.list()[2] return tostring(side.bounds) .. ' ' .. tostring(side.primary) .. ' ' .. #side.modes .. ' ' .. side.modes[2].width .. 'x' .. side.modes[2].height .. ' ' .. side.modes[2].refreshRate .. ' ' .. side.modes[2].scale"), "Rect(1920.0, 0.0, 1280.0, 1024.0) false 2 1024x768 75.0 1.0");

    // The display of the window holds most of it, and a window placed by the id of a display lands there.
    EXPECT_EQ(fixture.lua("return display.current().id"), "main-id");
    fixture.host().setFrame({2200.0F, 100.0F, 640.0F, 480.0F});
    fixture.engine().handleEvent({.type = Event::Type::WindowMoved});
    EXPECT_EQ(fixture.lua("return display.current().name"), "side");
    fixture.runLua("window.place({anchor = 'topLeft', display = 'main-id'})");
    EXPECT_EQ(fixture.host().getFrame(), (math::Rect{0.0F, 25.0F, 640.0F, 480.0F}));
}

TEST_F(DisplayLuaTest, PublishesEveryDisplayThatConnectsDisconnectsOrChanges) {
    test::EngineFixture fixture;
    fixture.runLua("events = require('haylen.events') heard = {} "
                   "for _, name in ipairs({'displayConnected', 'displayDisconnected', 'displayChanged', 'windowDisplayChanged'}) do "
                   "events.on(name, function(info) heard[#heard + 1] = name .. ' ' .. info.display .. (info.name and (' ' .. info.name) or '') end) end");

    std::vector<Display> displays = twoDisplays();
    fixture.host().setDisplays(displays);
    fixture.engine().handleEvent({.type = Event::Type::DisplaysChanged});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "displayDisconnected headless headless, displayConnected main-id main, displayConnected side-id side, windowDisplayChanged main-id");

    // A display whose work area changed is published once, and a report without changes publishes nothing.
    fixture.runLua("heard = {}");
    displays[1].workArea.height = 990.0F;
    fixture.host().setDisplays(displays);
    fixture.engine().handleEvent({.type = Event::Type::DisplaysChanged});
    fixture.engine().handleEvent({.type = Event::Type::DisplaysChanged});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "displayChanged side-id side");

    // The window moves to the display that stays when the one that held it disconnects.
    fixture.runLua("heard = {}");
    fixture.host().setFrame({2000.0F, 0.0F, 640.0F, 480.0F});
    fixture.engine().handleEvent({.type = Event::Type::WindowMoved});
    fixture.host().setDisplays({displays[0]});
    fixture.host().setFrame({0.0F, 0.0F, 640.0F, 480.0F});
    fixture.engine().handleEvent({.type = Event::Type::DisplaysChanged});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "windowDisplayChanged side-id, displayDisconnected side-id side, windowDisplayChanged main-id");
    EXPECT_EQ(fixture.engine().getDisplays().getAll().size(), 1U);
    EXPECT_EQ(fixture.engine().getDisplays().find("side-id"), nullptr);
}

TEST_F(DisplayLuaTest, GoesFullscreenOnAChosenDisplayInAChosenMode) {
    test::EngineFixture fixture;
    fixture.host().setDisplays(twoDisplays());
    fixture.engine().handleEvent({.type = Event::Type::DisplaysChanged});
    fixture.runLua("display = require('haylen.display') window = require('haylen.window')");

    // The window covers the display it goes fullscreen on, and a mode of the display makes the fullscreen exclusive, at the highest rate of its size when it names none.
    fixture.runLua("window.setFullscreen(true, {display = 'side-id', mode = display.list()[2].modes[2]})");
    EXPECT_TRUE(fixture.host().isFullscreen());
    EXPECT_EQ(fixture.host().getFrame(), (math::Rect{1920.0F, 0.0F, 1280.0F, 1024.0F}));
    EXPECT_EQ(fixture.host().getFullscreenTarget().display, "side-id");
    ASSERT_TRUE(fixture.host().getFullscreenTarget().mode.has_value());
    EXPECT_EQ(fixture.host().getFullscreenTarget().mode->refreshRate, 75.0F);
    fixture.runLua("window.setFullscreen(true, {mode = {width = 3840, height = 2160}, display = 'main-id'})");
    EXPECT_EQ(fixture.host().getFullscreenTarget().mode->refreshRate, 120.0F);
    fixture.runLua("window.setFullscreen(false)");
    EXPECT_FALSE(fixture.host().isFullscreen());
    EXPECT_FALSE(fixture.host().getFullscreenTarget().mode.has_value());

    EXPECT_NE(fixture.lua("window.setFullscreen(true, {display = 'gone'})").find("There is no display \"gone\" to go fullscreen on."), std::string::npos);
    EXPECT_NE(fixture.lua("window.setFullscreen(true, {display = 'side-id', mode = {width = 800, height = 600, refreshRate = 60}})").find("The display \"side-id\" has no mode of 800 by 600 pixels at 60 frames per second."), std::string::npos);
    EXPECT_NE(fixture.lua("window.setFullscreen(true, {mode = {width = 0, height = 600}})").find("A fullscreen mode needs a positive \"width\" and \"height\" in pixels."), std::string::npos);
    EXPECT_NE(fixture.lua("window.setFullscreen(true, {screen = 1})").find("Unknown option \"screen\"."), std::string::npos);
    EXPECT_NE(fixture.lua("window.setFullscreen(false, {display = 'main-id'})").find("options only go with true"), std::string::npos);
}

TEST_F(DisplayLuaTest, ShowsAnOutputOnAnotherDisplay) {
    test::EngineFixture fixture;
    showOutput(fixture, 1280, 1024);
    ASSERT_EQ(fixture.host().getOutputRequests().size(), 1U);
    const HeadlessHost::OutputRequest& request = fixture.host().getOutputRequests().front();
    EXPECT_EQ(request.display, "side-id");
    EXPECT_FALSE(request.options.fullscreen);
    EXPECT_EQ(request.options.size, (math::Vec2{800.0F, 600.0F}));
    EXPECT_EQ(request.options.title, "Board");

    // The output draws its target into a surface of its own at the end of the frame, cleared to its color around the target.
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return output.state .. ' ' .. output.width .. 'x' .. output.height .. ' ' .. tostring(output.target ~= nil) .. ' ' .. table.concat(heard, ', ')"), "shown 1280x1024 true outputShown 1280");
    EXPECT_EQ(fixture.host().getGpu().getAddedSurfaces().size(), 1U);
    const std::vector<gpu::NullDevice::PassRecord>& passes = fixture.host().getGpu().getLastFrame().passes;
    const auto presented = std::ranges::find_if(passes, [](const gpu::NullDevice::PassRecord& pass) { return pass.surface == 1; });
    ASSERT_NE(presented, passes.end());
    EXPECT_TRUE(presented->swapchain);
    EXPECT_EQ(presented->draws, 1);
    EXPECT_FLOAT_EQ(presented->clear.b, 0x30 / 255.0F);

    // A surface that resizes gives the output a target of its size, and one that goes away hides the output until it comes back.
    fixture.runLua("heard = {}");
    report(fixture, 640, 512);
    fixture.host().setOutputReport(1, {});
    fixture.engine().handleEvent({.type = Event::Type::OutputChanged, .output = 1});
    EXPECT_TRUE(fixture.host().getGpu().getAddedSurfaces().empty());
    report(fixture, 640, 512);
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return output.width .. ' ' .. table.concat(heard, ', ')"), "640 outputResized 640, outputHidden , outputShown 640");

    // The pointer over the output reaches the app in its points, apart from the input of the window.
    fixture.runLua("heard = {}");
    fixture.engine().handleEvent({.type = Event::Type::MouseDown, .mouseButton = input::MouseButton::Right, .position = {10.0F, 20.0F}, .output = 1});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "outputInput mouseDown");

    // A person that closes the window of the output ends it, and the platform forgets it once the GPU device let go of its surface.
    fixture.runLua("heard = {}");
    fixture.host().setOutputReport(1, {.ending = Output::Ending::Person});
    fixture.engine().handleEvent({.type = Event::Type::OutputChanged, .output = 1});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return output.state .. ' ' .. output.ending .. ' ' .. table.concat(heard, ', ')"), "closed person outputClosed person");
    EXPECT_TRUE(fixture.host().getGpu().getAddedSurfaces().empty());
    EXPECT_EQ(fixture.host().getClosedOutputs(), (std::vector<std::uint32_t>{1}));
    EXPECT_EQ(fixture.host().getSurfacesAtClose(), (std::vector<std::size_t>{0}));
    EXPECT_TRUE(fixture.engine().getOutputs().getAll().empty());
}

// A display that leaves the list ends its output whatever the platform reported, such as a monitor turned off that SDL still lists as connected.
TEST_F(DisplayLuaTest, EndsTheOutputOfADisplayThatLeaves) {
    test::EngineFixture fixture;
    showOutput(fixture, 1280, 1024);
    fixture.frames(1);
    fixture.runLua("heard = {} events.on('displayDisconnected', function(info) heard[#heard + 1] = 'displayDisconnected ' .. info.display end)");
    fixture.host().setDisplays({twoDisplays().front()});
    fixture.engine().handleEvent({.type = Event::Type::DisplaysChanged});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return output.state .. ' ' .. output.ending .. ' ' .. table.concat(heard, ', ')"), "closed disconnected displayDisconnected side-id, outputClosed disconnected");
    EXPECT_EQ(fixture.host().getClosedOutputs(), (std::vector<std::uint32_t>{1}));
    EXPECT_EQ(fixture.host().getSurfacesAtClose(), (std::vector<std::size_t>{0}));
    EXPECT_TRUE(fixture.engine().getOutputs().getAll().empty());
}

TEST_F(DisplayLuaTest, PresentsOutputsWithoutAllocating) {
    test::EngineFixture fixture;
    showOutput(fixture, 1280, 1024);
    fixture.frames(3);
    const test::AllocationTracker tracker;
    fixture.frames(5);
    EXPECT_EQ(tracker.getTotal(), 0U);
}

TEST_F(DisplayLuaTest, OpensOutputsOnlyWhereTheyCanShow) {
    test::EngineFixture fixture;
    showOutput(fixture, 1280, 1024);
    EXPECT_NE(fixture.lua("display.open('main-id')").find("The display \"main-id\" shows the window of the app, so it shows no output."), std::string::npos);
    EXPECT_NE(fixture.lua("display.open('gone')").find("There is no display \"gone\" to open an output on."), std::string::npos);
    EXPECT_NE(fixture.lua("display.open('side-id')").find("The display \"side-id\" shows another output already."), std::string::npos);
    EXPECT_NE(fixture.lua("display.open('side-id', {width = 0})").find("The window of an output needs a positive \"width\" and \"height\" in points."), std::string::npos);
    EXPECT_NE(fixture.lua("display.open('side-id', {size = 3})").find("Unknown option \"size\"."), std::string::npos);

    // The app that closes an output hears it once, and a platform that cannot show an output ends it with the reason.
    fixture.runLua("heard = {} output:close() output:close()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return output.ending .. ' ' .. table.concat(heard, ', ')"), "app outputClosed app");
    fixture.runLua("heard = {} refused = display.open('side-id')");
    fixture.host().setOutputReport(2, {.ending = Output::Ending::Unsupported, .reason = "No outputs here."});
    fixture.engine().handleEvent({.type = Event::Type::OutputChanged, .output = 2});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return refused.state .. ' ' .. table.concat(heard, ', ')"), "closed outputClosed unsupported");
}

} // namespace haylen::platform
