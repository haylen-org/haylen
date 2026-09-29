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
#include "support/EngineFixture.hpp"

namespace haylen::core {

TEST(CoreLuaTest, ExposesEngineInformationAndClock) {
    test::EngineFixture fixture;
    fixture.runLua("haylen = require('haylen')");

    EXPECT_EQ(fixture.lua("return haylen.platform .. ' ' .. haylen.backend"), "headless dummy");
    EXPECT_EQ(fixture.lua("return haylen.config.name .. ' ' .. haylen.config.design.width"), "Test App 1920.0");
    EXPECT_EQ(fixture.lua("return type(haylen.version)"), "string");

    fixture.frames(3, 0.5);
    EXPECT_EQ(fixture.lua("return haylen.frame()"), "3");
    EXPECT_EQ(fixture.lua("return haylen.delta() > 0 and haylen.unscaledDelta() > 0 and haylen.time() > 0"), "true");

    fixture.runLua("haylen.setTimeScale(0.5)");
    EXPECT_EQ(fixture.lua("return haylen.timeScale()"), "0.5");
    EXPECT_DOUBLE_EQ(fixture.engine().getClock().getTimeScale(), 0.5);
    fixture.runLua("haylen.setTimeScale(-2)");
    EXPECT_EQ(fixture.lua("return haylen.timeScale()"), "0.0");

    fixture.runLua("haylen.reportError('from lua')");
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "from lua");
    ASSERT_EQ(fixture.engine().getError()->getFrames().size(), 1U);
    EXPECT_EQ(fixture.engine().getError()->getFrames()[0].function, "main chunk");
    fixture.runLua("haylen.quit()");
    EXPECT_FALSE(fixture.engine().isRunning());
    EXPECT_TRUE(fixture.host().isQuitRequested());
}

TEST(CoreLuaTest, ReportsTheFixedStepClock) {
    test::EngineFixture fixture({{"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "fixedRate": 30})"}});
    fixture.runLua("haylen = require('haylen')");
    EXPECT_EQ(fixture.lua("return haylen.config.fixedRate"), "30.0");
    EXPECT_EQ(fixture.lua("return haylen.fixedStep() == 1 / 30"), "true");

    fixture.frames(1, 1.0 / 60.0);
    EXPECT_EQ(fixture.lua("return haylen.interpolation()"), "0.5");
    fixture.frames(1, 1.0 / 60.0);
    EXPECT_EQ(fixture.lua("return haylen.interpolation()"), "0.0");
}

TEST(CoreLuaTest, WritesLogsAtTheCurrentLevel) {
    test::EngineFixture fixture;
    std::vector<std::pair<core::Log::Level, std::string>> lines;
    const std::uint64_t listener = core::Log::addListener([&lines](core::Log::Level level, std::string_view line) { lines.emplace_back(level, std::string(line)); });
    fixture.runLua("log = require('haylen.log')");
    EXPECT_EQ(fixture.lua("return log.level()"), "info");

    fixture.runLua("log.debug('hidden', 1) log.info('shown', true) log.warning('careful') log.error({})");
    core::Log::write(core::Log::Level::Debug, "hidden from C++");
    fixture.runLua("require('log').debug('hidden from Varn')");
    fixture.runLua("log.setLevel('debug') log.debug('visible')");
    EXPECT_EQ(core::Log::getLevel(), core::Log::Level::Debug);
    EXPECT_EQ(fixture.lua("return log.level()"), "debug");
    fixture.runLua("log.setLevel('error') log.warning('dropped')");
    EXPECT_EQ(fixture.lua("return log.level()"), "error");

    core::Log::removeListener(listener);
    core::Log::setLevel(core::Log::Level::Info);
    ASSERT_EQ(lines.size(), 4U);
    EXPECT_EQ(lines[0].first, core::Log::Level::Info);
    EXPECT_NE(lines[0].second.find("shown\ttrue"), std::string::npos);
    EXPECT_EQ(lines[1].first, core::Log::Level::Warning);
    EXPECT_EQ(lines[2].first, core::Log::Level::Error);
    EXPECT_EQ(lines[3].first, core::Log::Level::Debug);
    EXPECT_NE(lines[3].second.find("visible"), std::string::npos);
    EXPECT_NE(fixture.lua("log.setLevel('loud')").find("bad argument #1 to 'setLevel' (unknown value 'loud')"), std::string::npos);
}

TEST(CoreLuaTest, SchedulesTimersOnTheFrameClock) {
    test::EngineFixture fixture;
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
    test::EngineFixture fixture;
    fixture.runLua("window = require('haylen.window')");
    EXPECT_EQ(fixture.lua("local w, h = window.size() return w .. 'x' .. h"), "1920.0x1080.0");
    EXPECT_EQ(fixture.lua("return window.dpiScale()"), "1.0");

    fixture.runLua("window.setTitle('Tiny Island') window.setFullscreen(true) window.setCursor('pointing_hand') window.setCursorVisible(false) window.setMouseLocked(true) window.showKeyboard(true) window.setClipboard('copied')");
    EXPECT_EQ(fixture.host().getTitle(), "Tiny Island");
    EXPECT_EQ(fixture.lua("return window.fullscreen()"), "true");
    EXPECT_EQ(fixture.host().getCursor(), platform::Window::Cursor::PointingHand);
    EXPECT_FALSE(fixture.host().isCursorVisible());
    EXPECT_TRUE(fixture.host().isMouseLocked());
    EXPECT_TRUE(fixture.host().isKeyboardVisible());
    EXPECT_EQ(fixture.lua("return window.clipboard()"), "copied");
    EXPECT_NE(fixture.lua("window.setCursor('spinning')").find("error: "), std::string::npos);

    EXPECT_EQ(fixture.lua("return window.resizable()"), "true");
    fixture.runLua("window.setResizable(false)");
    EXPECT_FALSE(fixture.host().isResizable());
    EXPECT_EQ(fixture.lua("return window.resizable()"), "false");
}

TEST(CoreLuaTest, ReadsAndLocksTheOrientation) {
    test::EngineFixture fixture;
    fixture.runLua("window = require('haylen.window') events = require('haylen.events') heard = {} events.on('window_orientation_changed', function(info) heard[#heard + 1] = info.orientation end)");
    EXPECT_EQ(fixture.lua("return window.orientation()"), "landscape");

    fixture.runLua("window.lockOrientation('portrait')");
    EXPECT_EQ(fixture.host().getOrientationLock(), platform::Orientation::Portrait);
    fixture.host().resize({1080.0F, 1920.0F});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return window.orientation() .. ' ' .. table.concat(heard, ',')"), "portrait portrait");
    EXPECT_NE(fixture.lua("window.lockOrientation('sideways')").find("error: "), std::string::npos);
}

TEST(CoreLuaTest, HearsTheKeyboardAndTheNetwork) {
    test::EngineFixture fixture;
    fixture.runLua("haylen = require('haylen') events = require('haylen.events') heard = {} for _, name in ipairs({'keyboard_shown', 'keyboard_hidden', 'network_offline', 'network_online'}) do events.on(name, function(info) heard[#heard + 1] = name .. (info and (' ' .. info.y .. ' ' .. info.height) or '') .. ' ' .. haylen.networkState() end) end");
    EXPECT_EQ(fixture.lua("return haylen.networkState()"), "unknown");
    fixture.engine().handleEvent({.type = platform::Event::Type::KeyboardChanged, .keyboardFrame = {0.0F, 700.0F, 1920.0F, 380.0F}});
    fixture.engine().handleEvent({.type = platform::Event::Type::NetworkChanged, .online = false});
    fixture.engine().handleEvent({.type = platform::Event::Type::KeyboardChanged});
    fixture.engine().handleEvent({.type = platform::Event::Type::NetworkChanged, .online = true});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "keyboard_shown 700.0 380.0 unknown, network_offline offline, keyboard_hidden offline, network_online online");
}

TEST(CoreLuaTest, AppliesTheResizableSettingOfAppJson) {
    test::EngineFixture fixture({{"app.json", R"({"window": {"resizable": false}})"}});
    EXPECT_FALSE(fixture.host().isResizable());
}

TEST(CoreLuaTest, ConvertsViewportCoordinates) {
    test::EngineFixture fixture;
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
    test::EngineFixture fixture({{"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "design": {"scaling": "fit"}})"}});
    fixture.host().resize({1000.0F, 540.0F});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(require('haylen.viewport').pixelRect())"), "Rect(20.0, 0.0, 960.0, 540.0)");
}

TEST(CoreLuaTest, RunsLuaScenesThroughTheSceneManager) {
    test::EngineFixture fixture;
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
    platform::Event key{.type = platform::Event::Type::KeyDown, .key = input::Key::Space};
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
    test::EngineFixture fixture;
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
    test::EngineFixture fixture;
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
        test::EngineFixture strict({{"source/main.lua", "require('haylen.scene').push(setmetatable({}, {__index = function(_, key) error('no field ' .. key) end}))"}});
        strict.frames(2);
        ASSERT_NE(strict.engine().getError(), nullptr);
        EXPECT_NE(std::string_view(strict.engine().getError()->what()).find("no field processMode"), std::string::npos);
    }

    test::EngineFixture opaque({{"source/main.lua", "require('haylen.scene').push(setmetatable({}, {__index = function(_, key) if key == 'transparent' then error('no field transparent') end end}))"}});
    opaque.frames(2);
    ASSERT_NE(opaque.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(opaque.engine().getError()->what()).find("no field transparent"), std::string::npos);
}

TEST(CoreLuaTest, SceneErrorsStopTheAppWithAMessage) {
    test::EngineFixture fixture({{"source/main.lua", "require('haylen.scene').push({update = function() error('scene exploded') end})"}});
    fixture.frames(2);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("scene exploded"), std::string::npos);
    EXPECT_EQ(fixture.engine().getError()->getFrames().size(), 2U);
}

TEST(JobsLuaTest, SplitsLongWorkAcrossFrames) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        jobs = require('haylen.jobs')
        async = require('async')
        jobs.setBudget(1)
        progress, other, result = 0, 0, nil
        local sum = jobs.spawn(function(limit)
            local total = 0
            for i = 1, limit do
                total = total + i
                progress = i
                jobs.checkpoint()
            end
            return total
        end, 300000)
        jobs.spawn(function() for i = 1, 300000 do other = i jobs.checkpoint() end end)
        async.spawn(function() result = sum:await() end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return jobs.running() .. ' ' .. jobs.budget()"), "2 1.0");

    // The first job uses the whole budget of the first frame, and the second one gets the next frame first.
    fixture.frames(1);
    const std::string first = fixture.lua("return progress .. ' ' .. other");
    EXPECT_NE(first.substr(0, first.find(' ')), "0");
    EXPECT_EQ(first.substr(first.find(' ') + 1), "0");
    EXPECT_NE(first.substr(0, first.find(' ')), "300000");
    fixture.frames(1);
    EXPECT_NE(fixture.lua("return other"), "0");

    for (int frame = 0; frame < 5000 && fixture.lua("return tostring(result)") == "nil"; ++frame) {
        fixture.frames(1);
    }
    EXPECT_EQ(fixture.lua("return result"), "45000150000");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(JobsLuaTest, RejectsFailuresAndMisuse) {
    test::EngineFixture fixture;
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
    EXPECT_NE(outcomes.find("may only pause at jobs.checkpoint"), std::string::npos) << outcomes;
    EXPECT_EQ(fixture.lua("return jobs.running()"), "0");

    EXPECT_NE(fixture.lua("jobs.checkpoint()").find("only runs inside a job"), std::string::npos);
    EXPECT_NE(fixture.lua("jobs.setBudget(0)").find("positive number"), std::string::npos);
}

TEST(JobsLuaTest, StartsWithMoreArgumentsThanTheMinimumStack) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        local values = {}
        for i = 1, 300 do values[i] = i end
        require('async').spawn(function()
            result = require('haylen.jobs').spawn(function(...) return select('#', ...) + select(300, ...) end, table.unpack(values)):await()
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(result)") != "nil"; }));
    EXPECT_EQ(fixture.lua("return result"), "600");
}

TEST(SignalLuaTest, ConnectsEmitsAndDisconnects) {
    test::EngineFixture fixture;
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

} // namespace haylen::core
