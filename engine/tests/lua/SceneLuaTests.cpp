#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/platform/Bridge.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::core {

TEST(SceneLuaTest, ManagesTheStackWithPromisesAndHooks) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        scene = require('haylen.scene')
        async = require('async')
        log = {}
        local function make(name)
            return {
                name = name,
                enter = function() log[#log + 1] = name .. ' enter' end,
                exit = function() log[#log + 1] = name .. ' exit' end,
                exitTransitionStarted = function() log[#log + 1] = name .. ' started' end,
                enterTransitionFinished = function() log[#log + 1] = name .. ' finished' end,
            }
        end
        root, a, b = make('root'), make('a'), make('b')
        results = {}
        scene.push(root)
        scene.push(a)
        async.spawn(function()
            local done = scene.push(b, {duration = 0.4, ease = 'quadInOut', blockInput = false, onComplete = function(done) results[#results + 1] = 'callback ' .. tostring(done) end}):await()
            results[#results + 1] = tostring(done)
        end)
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return scene.size() .. ' ' .. tostring(scene.transitioning())"), "2 true");
    fixture.frames(10, 0.05);
    EXPECT_EQ(fixture.lua("return scene.size() .. ' ' .. table.concat(results, ', ')"), "3 callback true, true");
    EXPECT_EQ(fixture.lua("return scene.at(1).name .. scene.at(3).name .. tostring(scene.at(4)) .. tostring(scene.at(0)) .. #scene.list() .. scene.list()[2].name"), "rootbnilnil3a");

    fixture.runLua("log = {} scene.popToRoot()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ') .. ' | ' .. scene.size()"), "b started, b exit, a exit, root finished | 1");
    fixture.runLua("scene.push(a) scene.push(b)");
    fixture.frames(1);
    fixture.runLua("log = {} scene.popTo(2)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ') .. ' | ' .. scene.top().name"), "b started, b exit, a finished | a");

    fixture.runLua("dropped = nil async.spawn(function() dropped = scene.replace(b, {duration = 1}):await() end) scene.clear()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(dropped) .. ' ' .. scene.size()"), "false 0");
    EXPECT_NE(fixture.lua("scene.push({}, {duration = 1, onComplete = 3})").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("scene.push({}, {fade = 1})").find("Unknown option \"fade\""), std::string::npos);
}

TEST(SceneLuaTest, DrawsCustomTransitionsAndHearsThePause) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        haylen = require('haylen')
        scene = require('haylen.scene')
        graphics2d = require('haylen.graphics2d')
        log = {}
        local function note(text) return function() log[#log + 1] = text end end
        wipe = {
            switchProgress = 0.25,
            render = function(self, progress, outgoing, incoming)
                graphics2d.beginScreen()
                graphics2d.draw(outgoing, 0, 0, {pivotX = 0, pivotY = 0})
                graphics2d.draw(incoming, 0, 0, {pivotX = 0, pivotY = 0, width = 1920 * progress, height = 1080, source = {0, 0, incoming.width * progress, incoming.height}})
                log[#log + 1] = string.format('draw %.1f %dx%d', progress, outgoing.width, incoming.height)
            end,
        }
        game = {paused = note('game paused'), unpaused = note('game unpaused')}
        menu = {processMode = 'whenPaused', transparent = true, enter = note('menu enter'), paused = note('menu paused'), unpaused = note('menu unpaused')}
        scene.push(game)
    )");
    // clang-format on
    fixture.frames(1);
    fixture.runLua("scene.push(menu, {duration = 1, effect = wipe})");
    fixture.frames(3, 0.1);
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ')"), "draw 0.1 1920x1080, draw 0.2 1920x1080, menu enter, draw 0.3 1920x1080");
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    fixture.runLua("log = {} haylen.setPaused(true) haylen.setPaused(false)");
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ')"), "game paused, menu unpaused, game unpaused, menu paused");
    EXPECT_NE(fixture.lua("scene.pop({duration = 1, effect = {}})").find("A transition effect needs a \"render\" method."), std::string::npos);
    EXPECT_NE(fixture.lua("scene.pop({duration = 1, effect = wipe, color = '#FFFFFF'})").find("takes no color or direction"), std::string::npos);
    EXPECT_NE(fixture.lua("scene.pop({duration = 1, effect = wipe, direction = 'up'})").find("takes no color or direction"), std::string::npos);
    EXPECT_NE(fixture.lua("scene.pop({effect = {switchProgress = 2, render = wipe.render}})").find("between 0 and 1"), std::string::npos);
    EXPECT_NE(fixture.lua("scene.pop({effect = {switchProgress = 0.5, exitProgress = 0.75, render = wipe.render}})").find("takes no \"exitProgress\""), std::string::npos);
    EXPECT_NE(fixture.lua("scene.pop({effect = {switchProgress = 0, exitProgress = 2, render = wipe.render}})").find("needs an \"exitProgress\" between 0 and 1"), std::string::npos);
    EXPECT_NE(fixture.lua("scene.pop({effect = 3})").find("must be the name of a built-in effect or a table"), std::string::npos);
    EXPECT_NE(fixture.lua("scene.pop({duration = 1, effect = 'swirl'})").find("unknown value 'swirl'"), std::string::npos);
    EXPECT_NE(fixture.lua("scene.pop({duration = 1, effect = 'wipe', direction = 'sideways'})").find("unknown value 'sideways'"), std::string::npos);
}

TEST(SceneLuaTest, PlaysBuiltInEffectsWithBothScenesAlive) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        scene = require('haylen.scene')
        log = {}
        function make(name)
            return {
                enter = function() log[#log + 1] = name .. ' enter' end,
                exit = function() log[#log + 1] = name .. ' exit' end,
                render = function() log[#log + 1] = name .. ' render' end,
            }
        end
        menu, game = make('menu'), make('game')
        scene.push(menu)
    )");
    // clang-format on
    fixture.frames(1);
    fixture.runLua("log = {} done = nil scene.replace(game, {effect = 'slideIn', direction = 'up', duration = 0.5, ease = 'quadOut', onComplete = function(completed) done = completed end})");
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ')"), "game enter, menu render, game render");
    fixture.frames(1, 0.25);
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ') .. ' ' .. tostring(done)"), "game enter, menu render, game render, menu exit, game render true");
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    // Every built-in effect draws both images through a frame of the transition, in every direction that applies.
    // clang-format off
    fixture.runLua(R"(
        local effects = {'fade', 'crossFade', 'moveIn', 'slideIn', 'push', 'shrinkGrow', 'flipX', 'flipY', 'zoomFlip', 'rotoZoom', 'jumpZoom', 'splitColumns', 'splitRows', 'turnOffTiles', 'fadeTiles', 'pageTurn', 'radialClockwise', 'radialCounterclockwise', 'wipe', 'inOut', 'outIn', 'iris', 'dissolve', 'pixelate'}
        local directions = {'left', 'right', 'up', 'down', 'upLeft', 'upRight', 'downLeft', 'downRight'}
        for index, name in ipairs(effects) do
            scene.replace(make(name), {effect = name, direction = directions[index % #directions + 1], color = '#203040', duration = 0.4})
        end
    )");
    // clang-format on
    for (int frame = 0; frame < 24 * 6; ++frame) {
        fixture.frames(1, 0.1);
    }
    EXPECT_EQ(fixture.lua("return scene.transitioning()"), "false");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(SceneLuaTest, LoadsScenesInCoroutinesBehindALoadingView) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(8, 8, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/world/grass.png", std::string(image.begin(), image.end())}, {"content/world/stone.png", std::string(image.begin(), image.end())}});
    // clang-format off
    fixture.runLua(R"(
        scene = require('haylen.scene')
        async = require('async')
        events = require('haylen.events')
        require('haylen.assets').defineGroup('world', {'world/'})
        log = {}
        phases = {}
        menu = {name = 'menu'}
        level = {
            load = function(self, context)
                self.context = context
                log[#log + 1] = 'load ' .. context.params.name
                context:progress(0.5, 'reading the map')
                async.sleep(30):await()
                context:preload('world'):await()
                log[#log + 1] = 'loaded'
            end,
            enter = function(self, params) log[#log + 1] = 'enter ' .. params.name end,
            enterTransitionFinished = function() log[#log + 1] = 'finished' end,
        }
        view = {
            enter = function() log[#log + 1] = 'view enter' end,
            exit = function() log[#log + 1] = 'view exit' end,
            update = function(self, dt, progress, message) self.progress, self.message, self.lowest = progress, message, math.min(self.lowest or 1, scene.loadingViewOpacity()) end,
            render = function(self, progress) self.rendered = progress end,
        }
        for _, name in ipairs({'sceneCoverStarted', 'sceneHoldStarted', 'sceneRevealFinished'}) do
            events.on(name, function(transfer) phases[#phases + 1] = name .. ' ' .. (transfer.from and transfer.from.name or 'none') .. '>' .. (transfer.to == level and 'level' or 'other') end)
        end
        scene.push(menu)
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(scene.state(level)) .. ' ' .. scene.state(menu)"), "nil active");
    fixture.runLua("async.spawn(function() done = scene.replace(level, {duration = 0.2, loading = view, params = {name = 'forest'}}):await() end)");
    fixture.frames(2, 0.05);
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ') .. ' | ' .. scene.state(level) .. ' ' .. scene.state(menu)"), "load forest, view enter | loading unloaded");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return view.progress .. ' ' .. view.message .. ' ' .. view.rendered"), "0.5 reading the map 0.5");
    EXPECT_EQ(fixture.lua("local value, message = scene.loadProgress(level) return value .. ' ' .. message"), "0.5 reading the map");

    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(done)") == "true"; }));
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ')"), "load forest, view enter, loaded, view exit, enter forest, finished");
    EXPECT_EQ(fixture.lua("return tostring(view.lowest < 1) .. ' ' .. scene.loadingViewOpacity()"), "true 0.0");
    EXPECT_EQ(fixture.lua("return table.concat(phases, ', ')"), "sceneCoverStarted menu>level, sceneHoldStarted menu>level, sceneRevealFinished menu>level");
    EXPECT_EQ(fixture.lua("return scene.state(level) .. ' ' .. scene.loadProgress(level)"), "active 1.0");
    EXPECT_NE(fixture.lua("level.context:progress(1)").find("haylen.SceneLoad\" was already released."), std::string::npos);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(SceneLuaTest, LoadsThatReturnPromisesOrFailRouteTheChange) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        scene = require('haylen.scene')
        async = require('async')
        events = require('haylen.events')
        log = {}
        menu = {name = 'menu'}
        failing = {name = 'failing', load = function() async.sleep(5):await() error('the map is missing') end}
        promising = {name = 'promising', load = function() return async.promise(function() async.sleep(5):await() return true end) end}
        rejecting = {name = 'rejecting', load = function() return async.promise(function() error('rejected load', 0) end) end}
        fallback = {name = 'fallback'}
        events.on('sceneLoadFailed', function(failure) log[#log + 1] = failure.scene.name .. ' failed' end)
        scene.push(menu)
    )");
    // clang-format on
    fixture.frames(1);

    // A failed push rejects its promise and keeps the menu.
    fixture.runLua("async.spawn(function() local ok, err = scene.push(failing, {duration = 0.2, onComplete = function(done) log[#log + 1] = 'complete ' .. tostring(done) end}):await() result = tostring(ok) .. ' ' .. err end)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(result ~= nil and not scene.transitioning())") == "true"; }));
    EXPECT_NE(fixture.lua("return result").find("nil test:"), std::string::npos);
    EXPECT_NE(fixture.lua("return result").find("the map is missing"), std::string::npos);
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ') .. ' | ' .. scene.top().name .. ' ' .. scene.state(failing)"), "failing failed, complete false | menu unloaded");

    // A load that returns a promise finishes with it.
    fixture.runLua("scene.push(promising)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return scene.top().name") == "promising"; }));

    // An error handler takes a failure after the replaced scene left and routes to another scene.
    fixture.runLua("scene.replace(rejecting, {duration = 0.2, onError = function(message) routed = message scene.replace(fallback) end})");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(scene.top() == fallback and not scene.transitioning())") == "true"; }));
    EXPECT_EQ(fixture.lua("return routed"), "rejected load");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(SceneLuaTest, AFirstSceneThatFailsToLoadShowsTheErrorScreen) {
    test::EngineFixture fixture({{"source/main.lua", "require('haylen.scene').push({load = function() error('no save file') end})"}});
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string(fixture.engine().getError()->what()).find("no save file"), std::string::npos);
    ASSERT_FALSE(fixture.engine().getError()->getFrames().empty());
    EXPECT_EQ(fixture.engine().getError()->getFrames().back().source, "source/main.lua");
}

TEST(SceneLuaTest, SpawnedTasksNeverResumeOnceTheirSceneUnloaded) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        haylen = require('haylen')
        scene = require('haylen.scene')
        async = require('async')
        platform = require('haylen.platform')
        log = {}
        Level = haylen.class('Level', scene.Scene)
        function Level:enter()
            self:spawn(function()
                local guard <close> = setmetatable({}, {__close = function() log[#log + 1] = 'closed' end})
                log[#log + 1] = 'waiting'
                platform.call('test.hold'):await()
                log[#log + 1] = 'resumed'
            end)
            scene.spawn(self, function()
                async.sleep(1):await()
                log[#log + 1] = 'quick'
            end)
        end
        level = Level()
        scene.push(level)
    )");
    // clang-format on
    fixture.frames(1);
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return log[#log]") == "quick"; }));
    fixture.runLua("scene.pop()");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ')"), "waiting, quick, closed");

    // The call the task waited on settles after the scene is gone and resumes nothing.
    ASSERT_EQ(fixture.host().getPlatformCalls().size(), 1U);
    fixture.engine().getPlatform().resolve(fixture.host().getPlatformCalls()[0].id, true, "null");
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ')"), "waiting, quick, closed");

    // A task that ends its own scene stops at its next wait, where it closes.
    // clang-format off
    fixture.runLua(R"(
        log = {}
        scene.push({enter = function(self)
            scene.spawn(self, function()
                local guard <close> = setmetatable({}, {__close = function() log[#log + 1] = 'closed' end})
                log[#log + 1] = 'clearing'
                scene.clear()
                async.sleep(1):await()
                log[#log + 1] = 'after clear'
            end)
        end})
    )");
    // clang-format on
    for (int step = 0; step < 5; ++step) {
        fixture.frames(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ') .. ' ' .. scene.size()"), "clearing, closed 0");
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    // An error in a task reaches the error screen with the stack of the task.
    fixture.runLua("scene.spawn({}, function() async.sleep(1):await() error('the task broke') end)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; }));
    EXPECT_NE(std::string(fixture.engine().getError()->what()).find("the task broke"), std::string::npos);
    EXPECT_NE(fixture.lua("scene.spawn(3, function() end)").find("An owner must be a table or a userdata"), std::string::npos);
}

TEST(SceneLuaTest, AFailingCloseOfACancelledTaskShowsTheError) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        scene = require('haylen.scene')
        async = require('async')
        owner = {}
        scene.spawn(owner, function()
            local guard <close> = setmetatable({}, {__close = function() error('the guard broke') end})
            async.sleep(1000):await()
        end)
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    fixture.runLua("owner = nil collectgarbage() collectgarbage()");
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string(fixture.engine().getError()->what()).find("the guard broke"), std::string::npos);
}

TEST(SceneLuaTest, EverythingASceneOwnsEndsWhenItUnloads) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        scene = require('haylen.scene')
        timer = require('haylen.timer')
        tween = require('haylen.tween')
        events = require('haylen.events')
        ui = require('haylen.ui')
        fired = {}
        owner = {
            load = function(self)
                events.on('ping', function() fired[#fired + 1] = 'event' end, {owner = self})
            end,
            enter = function(self)
                timer.after(0.05, function() fired[#fired + 1] = 'timer' end, {owner = self})
                self.value = {x = 0}
                tween.to(self.value, 0.5, {x = 1}, {owner = self})
                self.document = ui.mount(ui.label{text = 'hi'}, {owner = self})
            end,
            unload = function(self) fired[#fired + 1] = 'unload' end,
        }
        scene.push(owner)
    )");
    // clang-format on
    fixture.frames(1);
    fixture.runLua("events.emit('ping')");
    EXPECT_EQ(fixture.lua("return tostring(owner.document.mounted) .. ' ' .. table.concat(fired, ', ')"), "true event");

    fixture.runLua("scene.pop()");
    fixture.frames(1);
    const std::string value = fixture.lua("return owner.value.x");
    fixture.runLua("events.emit('ping')");
    fixture.frames(10, 0.05);
    EXPECT_EQ(fixture.lua("return tostring(owner.document.mounted) .. ' ' .. table.concat(fired, ', ')"), "false event, unload");
    EXPECT_EQ(fixture.lua("return owner.value.x"), value);
    EXPECT_NE(fixture.lua("ui.mount(ui.label{text = 'x'}, {owner = 3})").find("An owner must be a table or a userdata"), std::string::npos);
}

TEST(SceneLuaTest, PreloadsAndCancelsPreloads) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        scene = require('haylen.scene')
        async = require('async')
        level = {
            load = function(self, context) self.loadedWith = context.params async.sleep(5):await() end,
            enter = function(self, params) self.enteredWith = params end,
        }
        spare = {load = function()
            local guard <close> = setmetatable({}, {__close = function() spareClosed = true end})
            async.sleep(1000):await()
        end}
        async.spawn(function() result = scene.preload(level, 'hard'):await() end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return scene.state(level) .. ' ' .. level.loadedWith"), "loading hard");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(result)") == "true"; }));
    EXPECT_EQ(fixture.lua("return scene.state(level)"), "loaded");
    fixture.runLua("scene.replace(level)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return scene.state(level) .. ' ' .. level.enteredWith"), "active hard");

    fixture.runLua("async.spawn(function() cancelled = scene.preload(spare):await() end)");
    fixture.frames(1);
    fixture.runLua("scene.cancelPreload(spare)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(cancelled) .. ' ' .. scene.state(spare) .. ' ' .. tostring(spareClosed) .. ' ' .. tostring(scene.state({}))"), "false unloaded true nil");
    EXPECT_NE(fixture.lua("scene.cancelPreload(spare)").find("The scene is not preloaded."), std::string::npos);
    EXPECT_NE(fixture.lua("scene.preload(level)").find("already loaded or on the stack"), std::string::npos);
    EXPECT_NE(fixture.lua("scene.push({}, {loading = 3})").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("scene.pop({params = 1})").find("Unknown option \"params\""), std::string::npos);
    EXPECT_NE(fixture.lua("scene.push({}, {loadingDelay = -1})").find("cannot be negative"), std::string::npos);
    EXPECT_NE(fixture.lua("scene.push({}, {loadingFadeOut = -0.5})").find("the loading fade-out cannot be negative"), std::string::npos);
}

TEST(SceneLuaTest, AKeyHeldAcrossAChangePressesOnce) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        input = require('haylen.input')
        scene = require('haylen.scene')
        log = {}
        input.loadActions({actions = {{name = 'toggle', type = 'button', bindings = {'key:p'}}}})
        local menu = {update = function() if input.pressed('toggle') then log[#log + 1] = 'close' scene.pop() end end}
        scene.push({update = function() if input.pressed('toggle') then log[#log + 1] = 'open' scene.push(menu) end end})
    )");
    // clang-format on
    fixture.frames(2);

    // A tap of a few frames opens the menu, which is still held when the change ends and does not close it at once.
    Engine& engine = fixture.engine();
    engine.handleEvent({.type = platform::Event::Type::KeyDown, .key = input::Key::P});
    fixture.frames(3);
    engine.handleEvent({.type = platform::Event::Type::KeyUp, .key = input::Key::P});
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return table.concat(log, ' ') .. ' ' .. scene.size()"), "open 2");

    engine.handleEvent({.type = platform::Event::Type::KeyDown, .key = input::Key::P});
    fixture.frames(3);
    engine.handleEvent({.type = platform::Event::Type::KeyUp, .key = input::Key::P});
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return table.concat(log, ' ') .. ' ' .. scene.size()"), "open close 1");
}

} // namespace haylen::core
