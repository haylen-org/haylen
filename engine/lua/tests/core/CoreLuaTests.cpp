#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/FrameClock.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/platform/Event.hpp"
#include "support/LuaFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::lua::core {

TEST(CoreLuaTest, ExposesEngineInformationAndClock) {
    test::LuaFixture fixture;
    fixture.runLua("haylen = require('haylen')");

    EXPECT_EQ(fixture.lua("return haylen.platform .. ' ' .. haylen.graphicsApi"), "headless none");
    EXPECT_EQ(fixture.lua("return haylen.config.name .. ' ' .. haylen.config.design.width"), "Test App 1920.0");
    EXPECT_EQ(fixture.lua("return type(haylen.version)"), "string");

    fixture.frames(3, 0.5);
    EXPECT_EQ(fixture.lua("return haylen.frameIndex()"), "3");
    EXPECT_EQ(fixture.lua("return haylen.delta() > 0 and haylen.unscaledDelta() > 0 and haylen.elapsed() > 0"), "true");

    fixture.runLua("haylen.setTimeScale(0.5)");
    EXPECT_EQ(fixture.lua("return haylen.timeScale()"), "0.5");
    EXPECT_DOUBLE_EQ(fixture.engine().getClock().getTimeScale(), 0.5);
    fixture.runLua("haylen.setTimeScale(-2)");
    EXPECT_EQ(fixture.lua("return haylen.timeScale()"), "0.0");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(haylen.setTimeScale, math.huge) return message .. ' ' .. haylen.timeScale()"), "The time scale must be a finite number. 0.0");

    fixture.runLua("haylen.reportError('from lua')");
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "from lua");
    ASSERT_EQ(fixture.engine().getError()->getFrames().size(), 1U);
    EXPECT_EQ(fixture.engine().getError()->getFrames()[0].function, "main chunk");
    fixture.runLua("haylen.quit()");
    EXPECT_FALSE(fixture.engine().isRunning());
    EXPECT_TRUE(fixture.host().isQuitRequested());
    EXPECT_EQ(fixture.engine().getExitStatus(), 0);
}

TEST(CoreLuaTest, QuitsWithAnExitStatus) {
    test::LuaFixture fixture;
    fixture.runLua("haylen = require('haylen')");
    EXPECT_NE(fixture.lua("haylen.quit(1)").find("bad argument #1 to 'quit' (table expected, got number)"), std::string::npos);
    EXPECT_NE(fixture.lua("haylen.quit({code = 1})").find("Unknown option \"code\"."), std::string::npos);
    EXPECT_NE(fixture.lua("haylen.quit({status = 1.5})").find("The option \"status\" of \"quit\" is invalid: number has no integer representation."), std::string::npos);
    EXPECT_NE(fixture.lua("haylen.quit({status = -1})").find("The option \"status\" of \"quit\" is invalid: expected a non-negative integer."), std::string::npos);
    EXPECT_NE(fixture.lua("haylen.quit({status = 256})").find("The option \"status\" of \"quit\" is invalid: integer out of range."), std::string::npos);
    EXPECT_TRUE(fixture.engine().isRunning());
    EXPECT_EQ(fixture.lua("return haylen.canQuit()"), "true");

    fixture.runLua("haylen.quit({status = 3})");
    EXPECT_FALSE(fixture.engine().isRunning());
    EXPECT_EQ(fixture.engine().getExitStatus(), 3);
}

TEST(CoreLuaTest, LaunchesAnotherAppPackageOnceTheFrameEnds) {
    test::TemporaryDirectory guest;
    guest.write("haylen.json", R"({"name": "Guest", "identifier": "com.haylen.guest"})");
    test::LuaFixture fixture;
    EXPECT_EQ(fixture.lua("return tostring(require('haylen').launched())"), "false");
    EXPECT_FALSE(fixture.engine().getLaunchRequest().has_value());
    const std::string path = guest.getPath().generic_string();
    fixture.runLua("require('haylen').launch('" + path + "')");
    ASSERT_TRUE(fixture.engine().getLaunchRequest().has_value());
    EXPECT_EQ(fixture.engine().getLaunchRequest()->package, guest.getPath());
    EXPECT_EQ(fixture.engine().getLaunchRequest()->developmentServer, "");
    EXPECT_TRUE(fixture.engine().getLaunchRequest()->nativeAccess);
    EXPECT_FALSE(fixture.engine().getLaunchRequest()->address.has_value());
    fixture.runLua("require('haylen').launch('" + path + "', {address = 'https://example.com/apps/guest.zip'})");
    ASSERT_TRUE(fixture.engine().getLaunchRequest().has_value());
    EXPECT_EQ(fixture.engine().getLaunchRequest()->address, "https://example.com/apps/guest.zip");
    EXPECT_NE(fixture.lua("require('haylen').launch('" + path + "', {address = 'guest.zip'})").find("The address \"guest.zip\" of the launch holds no \"://\"."), std::string::npos);
    fixture.runLua("require('haylen').launch('" + path + "', {nativeAccess = false})");
    ASSERT_TRUE(fixture.engine().getLaunchRequest().has_value());
    EXPECT_FALSE(fixture.engine().getLaunchRequest()->nativeAccess);
    fixture.runLua("require('haylen').launch('" + path + "', {developmentServer = 'ws://192.168.0.10:8000/haylen/development?token=run'})");
    ASSERT_TRUE(fixture.engine().getLaunchRequest().has_value());
    EXPECT_EQ(fixture.engine().getLaunchRequest()->developmentServer, "ws://192.168.0.10:8000/haylen/development?token=run");
    EXPECT_NE(fixture.lua("require('haylen').launch('" + (guest.getPath() / "missing.zip").generic_string() + "')").find("No app package is at"), std::string::npos);
    EXPECT_NE(fixture.lua("require('haylen').launch('" + path + "', {developmentServer = 'http://192.168.0.10:8000/'})").find("The development server \"http://192.168.0.10:8000/\" is not an address that starts with \"ws://\" or \"wss://\"."), std::string::npos);
    EXPECT_NE(fixture.lua("require('haylen').launch('" + path + "', {server = 'ws://192.168.0.10:8000/'})").find("Unknown option \"server\"."), std::string::npos);
    fixture.host().setLaunched(true);
    EXPECT_EQ(fixture.lua("return tostring(require('haylen').launched())"), "true");
}

// The launch arguments reach the app as a new list each time, so a change to one list never reaches the next.
TEST(CoreLuaTest, ReturnsTheLaunchArgumentsOfTheApp) {
    test::LuaFixture fixture;
    EXPECT_EQ(fixture.lua("return #require('haylen').launchArguments()"), "0");
    fixture.host().setLaunchArguments({"--test", "PLG-011"});
    EXPECT_EQ(fixture.lua("local arguments = require('haylen').launchArguments() arguments[3] = 'changed' return table.concat(require('haylen').launchArguments(), ' ')"), "--test PLG-011");
}

// A covered app draws the one frame that stays on screen under the cover, and draws again once the cover ends.
TEST(CoreLuaTest, TellsWhetherTheFrameDraws) {
    test::LuaFixture fixture;
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(require('haylen').drawing())"), "true");

    fixture.host().getNativeViews().coverApp();
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(require('haylen').drawing())"), "true");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(require('haylen').drawing())"), "false");

    fixture.host().getNativeViews().uncoverApp();
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(require('haylen').drawing())"), "true");
}

// A list of downloaded apps shows each one by what its package says, read on the I/O pool.
TEST(CoreLuaTest, ReadsWhatAnAppPackageSaysAboutItself) {
    test::TemporaryDirectory guest;
    guest.write("haylen.json", R"({"name": "Guest", "identifier": "com.haylen.guest", "version": "2.1.0", "build": 7, "splash": {"logo": "brand/logo.svg"}})");
    guest.write("content/brand/logo.svg", "<svg/>");
    test::TemporaryDirectory plain;
    plain.write("haylen.json", R"({"name": "Plain", "identifier": "com.haylen.plain"})");
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        local async = require('async')
        local haylen = require('haylen')
        async.spawn(function()
            local info = haylen.packageInfo(')" + guest.getPath().generic_string() + R"('):await()
            local bare = haylen.packageInfo(')" + plain.getPath().generic_string() + R"('):await()
            local _, failure = haylen.packageInfo(')" + (plain.getPath() / "missing.zip").generic_string() + R"('):await()
            read = table.concat({info.name, info.identifier, info.version, info.build, info.logo, info.logoBytes, bare.name, bare.build, tostring(bare.logo), tostring(failure)}, '|')
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return read ~= nil") == "true"; }));
    const std::string read = fixture.lua("return read");
    EXPECT_EQ(read.substr(0, read.find("|nil|")), "Guest|com.haylen.guest|2.1.0|7|brand/logo.svg|<svg/>|Plain|1");
    EXPECT_NE(read.find("The app package could not be read. No app package is at"), std::string::npos);
}

TEST(CoreLuaTest, ReportsTheFixedStepClock) {
    test::LuaFixture fixture({{"haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "fixedRate": 30})"}});
    fixture.runLua("haylen = require('haylen')");
    EXPECT_EQ(fixture.lua("return haylen.config.fixedRate"), "30.0");
    EXPECT_EQ(fixture.lua("return haylen.fixedStep() == 1 / 30"), "true");

    fixture.frames(1, 1.0 / 60.0);
    EXPECT_EQ(fixture.lua("return haylen.interpolation()"), "0.5");
    fixture.frames(1, 1.0 / 60.0);
    EXPECT_EQ(fixture.lua("return haylen.interpolation()"), "0.0");
}

TEST(CoreLuaTest, WritesLogsAtTheCurrentLevel) {
    test::LuaFixture fixture;
    std::vector<std::pair<haylen::core::Log::Level, std::string>> lines;
    const std::uint64_t listener = haylen::core::Log::addListener([&lines](haylen::core::Log::Level level, std::string_view line) { lines.emplace_back(level, std::string(line)); });
    fixture.runLua("log = require('haylen.log')");
    EXPECT_EQ(fixture.lua("return log.level()"), "info");

    fixture.runLua("log.debug('hidden', 1) log.info('shown', true) log.warning('careful') log.error({})");
    haylen::core::Log::write(haylen::core::Log::Level::Debug, "hidden from C++");
    fixture.runLua("require('log').debug('hidden from Varn')");
    fixture.runLua("log.setLevel('debug') log.debug('visible')");
    EXPECT_EQ(haylen::core::Log::getLevel(), haylen::core::Log::Level::Debug);
    EXPECT_EQ(fixture.lua("return log.level()"), "debug");
    fixture.runLua("log.setLevel('error') log.warning('dropped')");
    EXPECT_EQ(fixture.lua("return log.level()"), "error");

    haylen::core::Log::removeListener(listener);
    haylen::core::Log::setLevel(haylen::core::Log::Level::Info);
    ASSERT_EQ(lines.size(), 4U);
    EXPECT_EQ(lines[0].first, haylen::core::Log::Level::Info);
    EXPECT_NE(lines[0].second.find("shown\ttrue"), std::string::npos);
    EXPECT_EQ(lines[1].first, haylen::core::Log::Level::Warning);
    EXPECT_EQ(lines[2].first, haylen::core::Log::Level::Error);
    EXPECT_EQ(lines[3].first, haylen::core::Log::Level::Debug);
    EXPECT_NE(lines[3].second.find("visible"), std::string::npos);
    EXPECT_NE(fixture.lua("log.setLevel('loud')").find("bad argument #1 to 'setLevel' (unknown value 'loud')"), std::string::npos);
}

TEST(CoreLuaTest, SchedulesTimersOnTheFrameClock) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        timer = require('haylen.timer')
        fired, ticks = 0, 0
        once = timer.after(0.1, function() fired = fired + 1 end)
        repeating = timer.every(0.1, function() ticks = ticks + 1 end, {count = 3})
        paused = timer.every(0.1, function() error('paused timers never fire') end)
        timer.pause(paused)
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return timer.active(once)"), "true");
    fixture.frames(20, 0.05);
    EXPECT_EQ(fixture.lua("return fired .. ' ' .. ticks"), "1 3");
    EXPECT_EQ(fixture.lua("return timer.active(once) or timer.active(repeating)"), "false");
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    fixture.runLua("timer.cancel(paused) failing = timer.after(0, function() error('timer failure') end)");
    EXPECT_EQ(fixture.lua("return timer.active(paused)"), "false");
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("timer failure"), std::string::npos);
    EXPECT_NE(fixture.lua("timer.after(1, 'not a function')").find("error: "), std::string::npos);
}

TEST(CoreLuaTest, ControlsTheWindow) {
    test::LuaFixture fixture;
    fixture.runLua("window = require('haylen.window')");
    EXPECT_EQ(fixture.lua("local w, h = window.framebufferSize() return w .. 'x' .. h"), "1920.0x1080.0");
    EXPECT_EQ(fixture.lua("return window.dpiScale()"), "1.0");

    fixture.runLua("window.setTitle('Tiny Island') window.setFullscreen(true) window.setCursor('pointingHand') window.setMouseMode('locked') window.setKeyboardVisible(true)");
    EXPECT_EQ(fixture.host().getTitle(), "Tiny Island");
    EXPECT_EQ(fixture.lua("return window.fullscreen()"), "true");
    EXPECT_EQ(fixture.host().getCursor(), haylen::platform::Window::Cursor::PointingHand);
    EXPECT_EQ(fixture.lua("return window.mouseMode()"), "locked");
    EXPECT_TRUE(fixture.host().isKeyboardVisible());
    EXPECT_NE(fixture.lua("window.setCursor('spinning')").find("error: "), std::string::npos);

    EXPECT_EQ(fixture.lua("return window.resizable()"), "true");
    fixture.runLua("window.setResizable(false)");
    EXPECT_FALSE(fixture.host().isResizable());
    EXPECT_EQ(fixture.lua("return window.resizable()"), "false");
}

TEST(CoreLuaTest, ReadsAndLocksTheOrientation) {
    test::LuaFixture fixture;
    fixture.runLua("window = require('haylen.window') events = require('haylen.events') heard = {} events.on('windowOrientationChanged', function(info) heard[#heard + 1] = info.orientation end)");
    EXPECT_EQ(fixture.lua("return window.orientation()"), "landscape");

    fixture.runLua("window.lockOrientation('portrait')");
    EXPECT_EQ(fixture.host().getOrientationLock(), haylen::platform::Orientation::Portrait);
    fixture.host().resize({1080.0F, 1920.0F});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return window.orientation() .. ' ' .. table.concat(heard, ',')"), "portrait portrait");
    EXPECT_NE(fixture.lua("window.lockOrientation('sideways')").find("error: "), std::string::npos);
}

TEST(CoreLuaTest, HearsTheKeyboardAndTheNetwork) {
    test::LuaFixture fixture;
    fixture.runLua("haylen = require('haylen') events = require('haylen.events') heard = {} for _, name in ipairs({'keyboardShown', 'keyboardHidden', 'networkOffline', 'networkOnline'}) do events.on(name, function(info) heard[#heard + 1] = name .. (info and (' ' .. info.y .. ' ' .. info.height) or '') .. ' ' .. haylen.networkState() end) end");
    EXPECT_EQ(fixture.lua("return haylen.networkState()"), "unknown");
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::KeyboardChanged, .keyboardFrame = {0.0F, 700.0F, 1920.0F, 380.0F}});
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::NetworkChanged, .online = false});
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::KeyboardChanged});
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::NetworkChanged, .online = true});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "keyboardShown 700.0 380.0 unknown, networkOffline offline, keyboardHidden offline, networkOnline online");
}

TEST(CoreLuaTest, AppliesTheResizableSettingOfHaylenJson) {
    test::LuaFixture fixture({{"haylen.json", R"({"window": {"resizable": false}})"}});
    EXPECT_FALSE(fixture.host().isResizable());
}

TEST(CoreLuaTest, ConvertsViewportCoordinates) {
    test::LuaFixture fixture;
    fixture.host().resize({960.0F, 540.0F});
    fixture.host().setSafeAreaInsets({.left = 20.0F, .top = 0.0F, .right = 0.0F, .bottom = 0.0F});
    fixture.frames(1);
    fixture.runLua("viewport = require('haylen.viewport')");

    EXPECT_EQ(fixture.lua("local w, h = viewport.designSize() return w .. 'x' .. h"), "1920.0x1080.0");
    EXPECT_EQ(fixture.lua("local x, y = viewport.pixelsPerUnit() return x .. 'x' .. y"), "0.5x0.5");
    EXPECT_EQ(fixture.lua("local x, y = viewport.toDesign(480, 270) return x .. ',' .. y"), "960.0,540.0");
    EXPECT_EQ(fixture.lua("local x, y = viewport.toFramebuffer(960, 540) return x .. ',' .. y"), "480.0,270.0");
    EXPECT_EQ(fixture.lua("return viewport.visibleRect().width"), "1920.0");
    EXPECT_EQ(fixture.lua("return viewport.safeRect().x"), "40.0");
    EXPECT_EQ(fixture.lua("return tostring(viewport.pixelRect())"), "Rect(0.0, 0.0, 960.0, 540.0)");
    EXPECT_EQ(fixture.lua("return viewport.scaling()"), "expand");
}

TEST(CoreLuaTest, PlacesLetterboxedViewportsInPixels) {
    test::LuaFixture fixture({{"haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "design": {"scaling": "fit"}})"}});
    fixture.host().resize({1000.0F, 540.0F});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(require('haylen.viewport').pixelRect())"), "Rect(20.0, 0.0, 960.0, 540.0)");
}

TEST(CoreLuaTest, RunsLuaScenesThroughTheSceneManager) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        scene = require('haylen.scene')
        calls = {}
        local function record(name) return function(self, value) calls[#calls + 1] = name .. (type(value) == 'table' and (':' .. value.type) or '') end end
        menu = {enter = record('menu.enter'), exit = record('menu.exit'), pause = record('menu.pause'), resume = record('menu.resume'), update = record('menu.update'), render = record('menu.render'), renderUi = record('menu.ui'), event = record('menu.event')}
        overlay = {transparent = true, enter = record('overlay.enter'), exit = record('overlay.exit'), fixedUpdate = record('overlay.fixed'), render = record('overlay.render')}
        scene.push(menu)
    )");
    // clang-format on

    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return scene.size() .. tostring(scene.top() == menu)"), "1true");
    fixture.runLua("calls = {} scene.push(overlay)");
    fixture.frames(1);
    haylen::platform::Event key{.type = haylen::platform::Event::Type::KeyDown, .key = haylen::input::Key::Space};
    fixture.engine().handleEvent(key);
    EXPECT_EQ(fixture.lua("return table.concat(calls, ',')"), "menu.pause,overlay.enter,menu.render,overlay.render,menu.ui");

    fixture.runLua("calls = {} scene.pop()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(calls, ',')"), "overlay.fixed,overlay.exit,menu.resume,menu.update,menu.render,menu.ui");

    fixture.runLua("calls = {} scene.replace(overlay, {duration = 0.2, color = '#000000'})");
    EXPECT_EQ(fixture.lua("return scene.transitioning()"), "true");
    fixture.frames(20, 0.05);
    EXPECT_EQ(fixture.lua("return scene.transitioning() or scene.top() == overlay"), "true");
    EXPECT_EQ(fixture.lua("return calls[1]"), "menu.update");

    fixture.runLua("scene.clear()");
    EXPECT_EQ(fixture.lua("return scene.size() .. tostring(scene.top())"), "0nil");
    EXPECT_NE(fixture.lua("scene.push('menu')").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("scene.push({}, {duration = 'long'})").find("error: "), std::string::npos);
}

TEST(CoreLuaTest, ScenesMayStartOverFromTheirOwnCallbacks) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        scene = require('haylen.scene')
        calls = {}
        title = {enter = function() calls[#calls + 1] = 'title.enter' end}
        gameOver = {enter = function() calls[#calls + 1] = 'gameOver.enter' scene.clear() scene.push(title) end}
        scene.push(gameOver)
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return table.concat(calls, ',')"), "gameOver.enter,title.enter");
    EXPECT_EQ(fixture.lua("return scene.size() .. tostring(scene.top() == title)"), "1true");

    // Starting over from the pause of the scene below ends the push that paused it.
    fixture.runLua("calls = {} scene.clear() scene.push({pause = function() scene.clear() scene.push(title) end})");
    fixture.frames(1);
    fixture.runLua("scene.push({enter = function() calls[#calls + 1] = 'intruder.enter' end})");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(calls, ',')"), "title.enter");
    EXPECT_EQ(fixture.lua("return scene.size() .. tostring(scene.top() == title)"), "1true");

    // A change that an exit callback requests while the stack clears is dropped with the others, so a stopped app leaves no scene behind.
    fixture.runLua("calls = {} scene.clear() scene.push({exit = function() scene.push(title) end})");
    fixture.frames(1);
    fixture.runLua("scene.clear()");
    EXPECT_EQ(fixture.lua("return tostring(scene.transitioning())"), "false");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return scene.size() .. ' ' .. #calls"), "0 0");
}

TEST(CoreLuaTest, EverySceneLeavesEvenWhenAnExitFails) {
    test::LuaFixture fixture;
    const std::string failingScenes = "scene = require('haylen.scene') for _, name in ipairs({'lower', 'upper'}) do scene.push({exit = function() error(name .. ' exit failed') end}) end";
    fixture.runLua(failingScenes);
    fixture.frames(1);
    EXPECT_NE(fixture.lua("scene.clear()").find("upper exit failed"), std::string::npos);
    EXPECT_TRUE(fixture.engine().getScenes().empty());

    // Stopping the engine, which a restart or quitting does, reports the failure instead of throwing out of it.
    fixture.runLua(failingScenes);
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getScenes().size(), 2U);
    fixture.engine().stop();
    EXPECT_TRUE(fixture.engine().getScenes().empty());
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("upper exit failed"), std::string::npos);
}

TEST(CoreLuaTest, SceneMetatablesThatRaiseErrorsStopTheAppWithAMessage) {
    // Strict tables raise an error for every field they lack, which the engine meets when it looks for optional callbacks.
    {
        test::LuaFixture strict({{"source/main.lua", "require('haylen.scene').push(setmetatable({}, {__index = function(_, key) error('no field ' .. key) end}))"}});
        strict.frames(2);
        ASSERT_NE(strict.engine().getError(), nullptr);
        EXPECT_NE(std::string_view(strict.engine().getError()->what()).find("no field processMode"), std::string::npos);
    }

    test::LuaFixture opaque({{"source/main.lua", "require('haylen.scene').push(setmetatable({}, {__index = function(_, key) if key == 'transparent' then error('no field transparent') end end}))"}});
    opaque.frames(2);
    ASSERT_NE(opaque.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(opaque.engine().getError()->what()).find("no field transparent"), std::string::npos);
}

TEST(CoreLuaTest, SceneErrorsStopTheAppWithAMessage) {
    test::LuaFixture fixture({{"source/main.lua", "require('haylen.scene').push({update = function() error('scene exploded') end})"}});
    fixture.frames(2);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("scene exploded"), std::string::npos);
    EXPECT_EQ(fixture.engine().getError()->getFrames().size(), 2U);
}

TEST(JobsLuaTest, SplitsLongWorkAcrossFrames) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        jobs = require('haylen.jobs')
        async = require('async')
        jobs.setBudget(1)
        progress, other, result = 0, 0, nil
        local sum = jobs.spawn(function()
            local total = 0
            for i = 1, 300000 do
                total = total + i
                progress = i
                jobs.checkpoint()
            end
            return total
        end)
        jobs.spawn(function() for i = 1, 300000 do other = i jobs.checkpoint() end end)
        async.spawn(function() result = sum:await() end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return jobs.runningCount() .. ' ' .. jobs.budget()"), "2 1.0");

    // The first job uses the whole budget of the first frame, and the second one gets the next frame first.
    fixture.frames(1);
    const std::string first = fixture.lua("return progress .. ' ' .. other");
    EXPECT_NE(first.substr(0, first.find(' ')), "0");
    EXPECT_EQ(first.substr(first.find(' ') + 1), "0");
    EXPECT_NE(first.substr(0, first.find(' ')), "300000");
    fixture.frames(1);
    EXPECT_NE(fixture.lua("return other"), "0");

    // The budget is time, which a loaded machine gives the jobs less of, so the jobs take as many frames as the machine needs. Each frame still moves a job on, so the jobs end within as many frames as they take steps.
    const long long steps = 600000;
    long long done = std::stoll(fixture.lua("return progress + other"));
    for (long long frame = 0; frame < steps && done < steps; ++frame) {
        fixture.frames(1);
        const long long now = std::stoll(fixture.lua("return progress + other"));
        ASSERT_GT(now, done) << "No job moved on in the frame " << frame << ".";
        done = now;
    }

    ASSERT_EQ(done, steps);
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return tostring(result)") != "nil"; }));
    EXPECT_EQ(fixture.lua("return result"), "45000150000");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(JobsLuaTest, RejectsFailuresAndMisuse) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        jobs = require('haylen.jobs')
        async = require('async')
        outcomes = {}
        local function watch(name, promise)
            async.spawn(function()
                local value, failure = promise:await()
                outcomes[#outcomes + 1] = name .. ' ' .. tostring(value) .. ' ' .. tostring(failure)
            end)
        end
        watch('empty', jobs.spawn(function() end))
        watch('broken', jobs.spawn(function() error('job exploded') end))
        watch('stray', jobs.spawn(function() coroutine.yield('hi') end))
        gate = {}
        watch('awaiting', jobs.spawn(function() gate.promise:await() awaited = true end))
        gate.promise = jobs.spawn(function() end)
    )");
    // clang-format on
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return tostring(awaited)"), "nil");
    const std::string outcomes = fixture.lua("return table.concat(outcomes, ' | ')");
    EXPECT_NE(outcomes.find("empty nil nil"), std::string::npos) << outcomes;
    EXPECT_NE(outcomes.find("job exploded"), std::string::npos) << outcomes;
    EXPECT_NE(outcomes.find("global 'error'"), std::string::npos) << outcomes;
    EXPECT_EQ(outcomes.find('\t'), std::string::npos) << outcomes;
    EXPECT_NE(outcomes.find("may only pause at \"jobs.checkpoint\""), std::string::npos) << outcomes;
    EXPECT_EQ(fixture.lua("return jobs.runningCount()"), "0");

    EXPECT_NE(fixture.lua("jobs.checkpoint()").find("only runs inside a job"), std::string::npos);
    EXPECT_NE(fixture.lua("jobs.setBudget(0)").find("positive number"), std::string::npos);
    EXPECT_EQ(fixture.lua("jobs.setBudget(0.0004) return jobs.budget()"), "0.001") << "A positive budget below a microsecond gets one.";
}

TEST(JobsLuaTest, CountsTheJobThatRuns) {
    test::LuaFixture fixture;
    fixture.runLua("jobs = require('haylen.jobs') counted = {} for index = 1, 2 do jobs.spawn(function() counted[index] = jobs.runningCount() end) end");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return counted[1] .. ' ' .. counted[2] .. ' ' .. jobs.runningCount()"), "2 1 0");
}

// A job never resumes once its owner ended, and its promise rejects without stopping the app.
TEST(JobsLuaTest, EndsWithItsOwner) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        jobs = require('haylen.jobs')
        async = require('async')
        scene = require('haylen.scene')
        steps = 0
        level = {}
        scene.push(level)
        local work = jobs.spawn(function()
            while true do
                steps = steps + 1
                jobs.checkpoint()
            end
        end, {owner = level})
        async.spawn(function() outcome = select(2, work:await()) end)
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_NE(fixture.lua("return steps"), "0");
    fixture.runLua("scene.pop()");
    fixture.frames(2);
    const std::string steps = fixture.lua("return steps");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return steps"), steps);
    EXPECT_EQ(fixture.lua("return outcome .. ' ' .. jobs.runningCount()"), "The job ended with its owner. 0");
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    EXPECT_EQ(fixture.lua("return select(2, pcall(jobs.spawn, function() end, {ownr = level}))"), "Unknown option \"ownr\".");
    EXPECT_NE(fixture.lua("return select(2, pcall(jobs.spawn, function() end, {owner = 4}))").find("An owner must be a table or a userdata"), std::string::npos);
}

TEST(SignalLuaTest, ConnectsEmitsAndDisconnects) {
    test::LuaFixture fixture;
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local signal = require('haylen.signal')
        local changed = signal.new()
        local seen = {}
        local first = changed:connect(function(name, value) seen[#seen + 1] = name .. '=' .. value end)
        changed:connect(function(name) seen[#seen + 1] = 'also ' .. name if name == 'hp' then changed:connect(function() seen[#seen + 1] = 'late' end) end end)
        changed:emit('hp', 3)
        first:disconnect()
        changed:emit('mp', 5)
        local size = changed.size
        changed:clear()
        changed:emit('gone', 0)
        return table.concat(seen, ', ') .. ' | ' .. size .. ' ' .. changed.size .. ' ' .. tostring(first.connected)
    )"), "hp=3, also hp, also mp, late | 2 0 false");
    // clang-format on
    EXPECT_NE(fixture.lua("local s = require('haylen.signal').new() s:connect(function() error('listener broke') end) s:emit()").find("listener broke"), std::string::npos);

    // Lua cannot reach the metatable of a bound type, so its finalizer never runs by hand and its members never move to another value.
    EXPECT_EQ(fixture.lua("local s = require('haylen.signal').new() return getmetatable(s) .. ' ' .. tostring(pcall(setmetatable, {}, getmetatable(s))) .. ' ' .. tostring(pcall(debug.setmetatable, s, nil)) .. ' ' .. s.size"), "haylen.Signal false false 0");
    EXPECT_EQ(fixture.lua("local s, values, seen = require('haylen.signal').new(), {}, 0 for i = 1, 300 do values[i] = i end s:connect(function(...) seen = seen + select('#', ...) end) s:connect(function(...) seen = seen + select(300, ...) end) s:emit(table.unpack(values)) return seen"), "600");

    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local s = require('haylen.signal').new()
        local calls, second = {}, nil
        s:connect(function() calls[#calls + 1] = 'first' second:disconnect() end)
        second = s:connect(function() calls[#calls + 1] = 'second' end)
        local kept = s:connect(function() calls[#calls + 1] = 'third' end)
        s:emit()
        s:clear()
        return table.concat(calls, ',') .. ' ' .. tostring(kept.connected)
    )"), "first,third false");
    // clang-format on
}

} // namespace haylen::lua::core
