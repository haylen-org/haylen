#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/platform/Event.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::core {

TEST(SignalLuaTest, ConnectsWithPriorityOnceOwnersAndDeferral) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        signal = require('haylen.signal')
        hit = signal.new('hit')
        calls = {}
        local function record(name) return function(...) calls[#calls + 1] = name .. ':' .. table.concat({...}, '/') end end
        hit:connect(record('normal'))
        hit:connect(record('first'), {priority = 5})
        hit:connect(record('once'), {once = true})
        later = hit:connect(record('later'), {deferred = true})
        owner = {}
        hit:connect(record('owned'), {owner = owner})
        hit:emit(1, 2)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return table.concat(calls, ' ')"), "first:1/2 normal:1/2 once:1/2 owned:1/2");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return calls[#calls] .. ' ' .. hit.size .. ' ' .. hit.emissionCount .. ' ' .. hit.name"), "later:1/2 4 1 hit");

    fixture.runLua("calls = {} owner = nil collectgarbage() collectgarbage() later.blocked = true hit:emit(3)");
    EXPECT_EQ(fixture.lua("return table.concat(calls, ' ') .. ' ' .. tostring(later.blocked)"), "first:3 normal:3 true");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return hit.size"), "3");

    fixture.runLua("calls = {} hit.blocked = true hit:emit(4) hit.blocked = false later.blocked = false later:disconnect() hit:emit(5)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(calls, ' ') .. ' ' .. tostring(hit.blocked)"), "first:5 normal:5 false");
    EXPECT_EQ(fixture.lua("local list = signal.list() return #list .. ' ' .. list[1].name .. ' ' .. list[1].listeners .. ' ' .. list[1].emissions .. ' ' .. list[1].stale"), "1 hit 2 4 0");
    EXPECT_NE(fixture.lua("hit:connect(function() end, {weak = true})").find("Unknown option \"weak\""), std::string::npos);
    EXPECT_NE(fixture.lua("hit:connect(function() end, {owner = 5})").find("An owner must be a table or a userdata"), std::string::npos);

    // A deferred once listener runs its one call at the end of the frame.
    fixture.runLua("calls = {} hit:connect(function(value) calls[#calls + 1] = 'deferred once:' .. value end, {once = true, deferred = true}) hit:emit(6) hit:emit(7)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(calls, ' ')"), "first:6 normal:6 first:7 normal:7 deferred once:6");
}

TEST(EventsLuaTest, PublishesAndSubscribesByName) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        events = require('haylen.events')
        calls = {}
        events.on('damage', function(target, amount) calls[#calls + 1] = 'any ' .. target .. ' ' .. amount end)
        events.on('damage', function(target) calls[#calls + 1] = 'shield ' .. target return target == 'hero' end, {priority = 10, channel = 'player'})
        events.on('damage', function(_, amount) calls[#calls + 1] = 'big ' .. amount end, {once = true, filter = function(_, amount) return amount > 5 end})
        first = events.emitTo('player', 'damage', 'hero', 3)
        second = events.emit('damage', 'goblin', 9)
        events.emit('damage', 'goblin', 10)
        events.post('damage', 'queued', 1)
        events.postTo('player', 'damage', 'late', 2)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return table.concat(calls, ', ')"), "shield hero, any goblin 9, big 9, any goblin 10");
    EXPECT_EQ(fixture.lua("return tostring(first) .. ' ' .. tostring(second)"), "true false");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(calls, ', ', 5)"), "any queued 1, shield late, any late 2");

    EXPECT_EQ(fixture.lua("local s = events.topics()[1] return s.name .. ' ' .. s.listeners .. ' ' .. s.emissions"), "damage 2 5");
    EXPECT_NE(fixture.lua("events.on('x', function() end, {channels = 'a'})").find("Unknown option \"channels\""), std::string::npos);
    EXPECT_NE(fixture.lua("events.on('x', function() end, {filter = 3})").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("events.on('boom', function() error('listener broke') end) events.emit('boom')").find("listener broke"), std::string::npos);
}

TEST(EventsLuaTest, ReceivesEngineLifecycleEvents) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        haylen = require('haylen')
        events = require('haylen.events')
        scene = require('haylen.scene')
        log = {}
        for _, name in ipairs({'paused', 'unpaused', 'appInactive', 'appActive', 'appBackground', 'windowResized', 'sceneEntered', 'sceneExited', 'sceneEnterTransitionFinished'}) do
            events.on(name, function(payload) log[#log + 1] = name .. (type(payload) == 'table' and (payload.width and (' ' .. payload.width) or (payload == menu and ' menu' or ' table')) or '') end)
        end
        menu = {}
        scene.push(menu)
    )");
    // clang-format on
    fixture.frames(1);
    fixture.runLua("haylen.setPaused(true) haylen.setPaused(false) scene.pop()");
    fixture.frames(1);
    fixture.engine().handleEvent({.type = platform::Event::Type::FocusLost});
    fixture.engine().handleEvent({.type = platform::Event::Type::Suspended});
    fixture.engine().handleEvent({.type = platform::Event::Type::Resized});
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ')"), "sceneEntered menu, sceneEnterTransitionFinished menu, paused, unpaused, sceneExited menu, appInactive, appBackground, windowResized 1920.0");
    EXPECT_EQ(fixture.lua("return haylen.appState() .. ' ' .. tostring(haylen.halted()) .. ' ' .. tostring(haylen.paused())"), "background true false");
    fixture.engine().handleEvent({.type = platform::Event::Type::Resumed});
    EXPECT_EQ(fixture.lua("return haylen.appState() .. ' ' .. tostring(haylen.halted())"), "inactive false");
    fixture.engine().handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_EQ(fixture.lua("return haylen.appState()"), "active");
}

TEST(EventsLuaTest, ScenesListenForAsLongAsTheyLive) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        haylen = require('haylen')
        scene = require('haylen.scene')
        signal = require('haylen.signal')
        timer = require('haylen.timer')
        events = require('haylen.events')
        changed = signal.new()
        heard = {}
        Level = haylen.class('Level', scene.Scene)
        function Level:enter()
            self:listen(changed, function(value) heard[#heard + 1] = 'signal ' .. value end)
            self:listen('score', function(value) heard[#heard + 1] = 'event ' .. value end)
            timer.every(0.1, function() heard[#heard + 1] = 'tick' end, {owner = self, count = 50})
        end
        level = Level()
        plain = {}
        scene.listen(plain, 'score', function(value) heard[#heard + 1] = 'plain ' .. value end, {priority = 1})
        scene.push(level)
    )");
    // clang-format on
    fixture.frames(1);
    fixture.runLua("changed:emit(1) events.emit('score', 2)");
    fixture.frames(6, 0.02);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "signal 1, plain 2, event 2, tick");
    fixture.runLua("heard = {} scene.pop()");
    fixture.frames(1);
    fixture.runLua("changed:emit(3) events.emit('score', 4)");
    fixture.frames(10, 0.02);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "plain 4");
    EXPECT_NE(fixture.lua("scene.listen({}, 5, function() end)").find("signal or event name expected"), std::string::npos);
    EXPECT_NE(fixture.lua("scene.listen(3, 'x', function() end)").find("An owner must be a table or a userdata"), std::string::npos);
}

TEST(EventsLuaTest, OwnerFunctionsDoNotKeepTheirOwnerAlive) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        events = require('haylen.events')
        tracker = setmetatable({}, {__mode = 'k'})
        do
            local enemy = {name = 'goblin'}
            tracker[enemy] = true
            events.on('turn', function() enemy.turns = (enemy.turns or 0) + 1 end, {owner = enemy})
        end
        collectgarbage() collectgarbage()
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(next(tracker)) .. ' ' .. require('haylen.events').topics()[1].listeners"), "nil 0");
}

TEST(EventsLuaTest, ListenersOfACollectedOwnerCountAsStaleUntilRemoved) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        events = require('haylen.events')
        signal = require('haylen.signal')
        tick = signal.new('tick')
        function counts()
            local topic
            for _, entry in ipairs(events.topics()) do
                if entry.name == 'pulse' then topic = entry end
            end
            local named = signal.list()[1]
            return topic.listeners .. ' ' .. topic.stale .. ' ' .. named.listeners .. ' ' .. named.stale
        end
        do
            local owner = {}
            events.on('pulse', function() end, {owner = owner})
            tick:connect(function() end, {owner = owner})
        end
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return counts()"), "1 0 1 0");

    fixture.runLua("collectgarbage() collectgarbage()");
    EXPECT_EQ(fixture.lua("return counts()"), "1 1 1 1");
    fixture.runLua("events.emit('pulse')");
    EXPECT_EQ(fixture.lua("return counts()"), "0 0 1 1");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return counts()"), "0 0 0 0");
}

TEST(CoreLuaTest, PausesAndConfiguresTheLifecycle) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        haylen = require('haylen')
        scene = require('haylen.scene')
        timer = require('haylen.timer')
        counts = {game = 0, menu = 0, timer = 0, menuTimer = 0, realTimer = 0}
        game = {update = function() counts.game = counts.game + 1 end}
        menu = {processMode = 'whenPaused', transparent = true, update = function() counts.menu = counts.menu + 1 end}
        timer.every(0.1, function() counts.timer = counts.timer + 1 end)
        timer.every(0.1, function() counts.menuTimer = counts.menuTimer + 1 end, {owner = menu})
        timer.every(0.1, function() counts.realTimer = counts.realTimer + 1 end, {processMode = 'always', unscaled = true})
        scene.push(game)
        scene.push(menu)
        haylen.setTimeScale(0)
        haylen.setPaused(true)
    )");
    // clang-format on
    fixture.frames(5, 0.1);
    EXPECT_EQ(fixture.lua("return counts.game .. ' ' .. counts.menu .. ' ' .. counts.timer .. ' ' .. counts.menuTimer .. ' ' .. counts.realTimer"), "0 5 0 0 5");
    fixture.runLua("haylen.setTimeScale(1)");
    fixture.frames(5, 0.1);
    EXPECT_EQ(fixture.lua("return counts.menu .. ' ' .. counts.timer .. ' ' .. counts.menuTimer"), "10 0 5");

    EXPECT_EQ(fixture.lua("local l = haylen.lifecycle() return tostring(l.pauseOnBackground) .. tostring(l.pauseOnFocusLoss) .. tostring(l.muteOnFocusLoss)"), "truefalsefalse");
    fixture.runLua("haylen.setLifecycle({pauseOnFocusLoss = true})");
    EXPECT_TRUE(fixture.engine().getLifecycle().pauseOnFocusLoss);
    EXPECT_TRUE(fixture.engine().getLifecycle().pauseOnBackground);
    EXPECT_EQ(fixture.lua("return haylen.config.lifecycle.pauseOnBackground"), "true");
    EXPECT_NE(fixture.lua("haylen.setLifecycle({pauseOnSleep = true})").find("Unknown option \"pauseOnSleep\""), std::string::npos);
    EXPECT_NE(fixture.lua("scene.push({processMode = 'sometimes'})").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("timer.after(1, function() end, {count = 2})").find("Unknown option \"count\""), std::string::npos);
}

TEST(CoreLuaTest, TimersAndTweensFollowTheModeOfTheirOwner) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        haylen = require('haylen')
        scene = require('haylen.scene')
        timer = require('haylen.timer')
        tween = require('haylen.tween')
        counts = {scene = 0, table = 0}
        level = {}
        music = {processMode = 'whenPaused'}
        box = {v = 0}
        scene.push(level)
        timer.every(0.1, function() counts.scene = counts.scene + 1 end, {owner = level})
        timer.every(0.1, function() counts.table = counts.table + 1 end, {owner = music})
        tween.to(box, 100, {v = 100}, {owner = level})
        haylen.setPaused(true)
    )");
    // clang-format on
    fixture.frames(5, 0.1);
    EXPECT_EQ(fixture.lua("return counts.scene .. ' ' .. counts.table .. ' ' .. box.v"), "0 5 0");

    fixture.runLua("level.processMode = 'always' music.processMode = 'pausable'");
    fixture.frames(5, 0.1);
    EXPECT_EQ(fixture.lua("return counts.scene .. ' ' .. counts.table .. ' ' .. tostring(box.v > 0)"), "5 5 true");

    fixture.runLua("level.processMode = nil moved = box.v");
    fixture.frames(5, 0.1);
    EXPECT_EQ(fixture.lua("return counts.scene .. ' ' .. tostring(box.v == moved)"), "5 true");
}

TEST(AutoloadTest, LoadsModulesBeforeMainAndRunsTheirCallbacks) {
    // clang-format off
    test::EngineFixture fixture({
        {"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "autoload": ["state.player-data"]})"},
        {"source/state/player-data.lua", R"(
            local data = {coins = 0, log = {}}
            function data:start() self.log[#self.log + 1] = 'start' end
            function data:update(dt) self.coins = self.coins + 1 end
            function data:event(event) self.log[#self.log + 1] = event.type end
            function data:stop() self.log[#self.log + 1] = 'stop' end
            return data
        )"},
        {"source/state/menu-music.lua", "return {processMode = 'whenPaused', ticks = 0, update = function(self) self.ticks = self.ticks + 1 end}"},
        {"source/main.lua", "startCoins = require('haylen').autoloads.playerData.coins sameTable = require('state.player-data') == require('haylen').autoloads.playerData"},
    });
    // clang-format on
    EXPECT_EQ(fixture.lua("return startCoins .. ' ' .. tostring(sameTable)"), "0 true");
    fixture.frames(3);
    fixture.engine().handleEvent({.type = platform::Event::Type::KeyDown, .key = input::Key::A});
    EXPECT_EQ(fixture.lua("local data = require('state.player-data') return data.coins .. ' ' .. table.concat(data.log, ',')"), "3 start,keyDown");

    fixture.runLua("haylen = require('haylen') music = haylen.autoload('soundtrack', 'state.menu-music')");
    fixture.frames(1);
    fixture.runLua("haylen.setPaused(true)");
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return music.ticks .. ' ' .. tostring(haylen.autoloads.soundtrack == music)"), "2 true");
    EXPECT_EQ(fixture.lua("return require('state.player-data').coins"), "4");
    EXPECT_NE(fixture.lua("haylen.autoload('soundtrack', 'state.player-data')").find("already exists"), std::string::npos);
    EXPECT_NE(fixture.lua("haylen.autoload('state.menu-music')").find("is already the autoload \"soundtrack\""), std::string::npos);
    EXPECT_NE(fixture.lua("haylen.autoload('missing.module')").find("missing/module"), std::string::npos);

    fixture.runLua("stopLog = require('state.player-data').log");
    fixture.engine().stop();
    EXPECT_EQ(fixture.lua("return stopLog[#stopLog]"), "stop");
}

TEST(AutoloadTest, StopsEveryAutoloadWhenOneFails) {
    // clang-format off
    test::EngineFixture fixture({
        {"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "autoload": ["first", "second"]})"},
        {"source/first.lua", "return {stop = function() stopped = 'first' end}"},
        {"source/second.lua", "return {stop = function() error('second cannot stop') end}"},
    });
    // clang-format on
    fixture.engine().stop();
    EXPECT_EQ(fixture.lua("return stopped"), "first");
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("second cannot stop"), std::string::npos);
}

TEST(AutoloadTest, ReportsModulesThatAreNotTables) {
    test::EngineFixture fixture({{"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "autoload": ["broken"]})"}, {"source/broken.lua", "return 5"}});
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("must return a table"), std::string::npos);
}

TEST(ClassLuaTest, BuildsClassesWithInheritanceAndMixins) {
    test::EngineFixture fixture;
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local class = require('haylen').class
        local Walker = {walk = function(self) return self.name .. ' walks' end}
        local Unit = class('Unit')
        function Unit:init(name, hp) self.name, self.hp = name, hp end
        function Unit:describe() return self.name .. ' ' .. self.hp end
        function Unit:__eq(other) return self.name == other.name end
        local Hero = class('Hero', Unit, Walker)
        function Hero:init(name) Hero.super.init(self, name, 100) self.level = 1 end
        function Hero:describe() return 'hero ' .. Unit.describe(self) end
        local hero = Hero('Ana')
        local other = Hero.new('Ana')
        local unit = Unit('Orc', 5)
        return table.concat({
            hero:describe(), hero:walk(), tostring(hero:is(Hero)), tostring(hero:is(Unit)), tostring(hero:is(Walker)), tostring(unit:is(Hero)),
            tostring(Hero:is(Unit)), tostring(hero == other), Hero.name, Hero.super.name, tostring(Hero), tostring(hero):sub(1, 5),
        }, ',')
    )"), "hero Ana 100,Ana walks,true,true,true,false,true,true,Hero,Unit,class Hero,Hero:");
    // clang-format on
    EXPECT_NE(fixture.lua("require('haylen').class(5)").find("string expected"), std::string::npos);
    EXPECT_NE(fixture.lua("require('haylen').class('A', {})").find("class expected"), std::string::npos);
    EXPECT_NE(fixture.lua("require('haylen').class('A'):include(3)").find("table expected"), std::string::npos);
}

} // namespace haylen::core
