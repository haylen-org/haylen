#include <gtest/gtest.h>

#include <lua.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "platform/DevelopmentSession.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua {

// Starts apps in development and saves their files the way an editor does, one batch per save.
class ModuleReloadTest : public ::testing::Test {
  protected:
    void TearDown() override {
        core::Log::removeListener(listener);
    }

    void start(std::map<std::string, std::string> files) {
        fixture.reset();
        fixture = std::make_unique<test::EngineFixture>(std::move(files), nullptr, test::EngineFixture::Options{.development = true});
        fixture->frames(1);
    }

    // Writes the files, queues their paths in the development session and runs the frame that applies them.
    void edit(const std::map<std::string, std::string>& files) {
        std::vector<std::string> paths;
        for (const auto& [path, source] : files) {
            fixture->package().setFile(path, test::TestFiles::bytes(source));
            paths.push_back(path);
        }
        fixture->host().getDevelopmentSession()->addChanges(paths);
        fixture->frames(1);
    }

    void remove(const std::string& path) {
        fixture->package().removeFile(path);
        const std::vector<std::string> paths{path};
        fixture->host().getDevelopmentSession()->addChanges(paths);
        fixture->frames(1);
    }

    std::string lua(const std::string& source) {
        return fixture->lua(source);
    }

    [[nodiscard]] bool restarted() const {
        return fixture->engine().isRestartRequested();
    }

    [[nodiscard]] std::string error() const {
        const Error* failure = fixture->engine().getError();
        return failure != nullptr ? failure->what() : "";
    }

    void recordLog() {
        // clang-format off
        listener = core::Log::addListener([this](core::Log::Level, std::string_view line) {
            const std::scoped_lock lock(logMutex);
            lines.emplace_back(line);
        });
        // clang-format on
    }

    [[nodiscard]] bool logged(std::string_view text) {
        const std::scoped_lock lock(logMutex);
        return std::ranges::any_of(lines, [text](const std::string& line) { return line.find(text) != std::string::npos; });
    }

    std::unique_ptr<test::EngineFixture> fixture;
    std::uint64_t listener = 0;
    std::mutex logMutex;
    std::vector<std::string> lines;
};

class HotReloadLuaTest : public ModuleReloadTest {};

// clang-format off
TEST_F(ModuleReloadTest, ReplacesFunctionsAndKeepsUpvalueState) {
    start({{"source/counter.lua", R"(
local M = {}
local count = 0
function M.bump() count = count + 1 return count end
function M.label() return 'v1 ' .. count end
return M
)"}, {"source/main.lua", "counter = require('counter') counter.bump() counter.bump()"}});
    EXPECT_EQ(lua("return counter.label()"), "v1 2");

    edit({{"source/counter.lua", R"(
local M = {}
local count = 0
function M.bump() count = count + 1 return count end
function M.label() return 'v2 ' .. count end
return M
)"}});
    EXPECT_EQ(lua("return counter.label()"), "v2 2");
    EXPECT_EQ(lua("return counter.bump()"), "3");
    EXPECT_EQ(lua("return require('counter') == counter"), "true");
    EXPECT_FALSE(restarted());
    EXPECT_EQ(error(), "");
}

TEST_F(ModuleReloadTest, TakesEditedLiteralsAndKeepsChangedState) {
    start({{"source/tuning.lua", R"(
local M = {speed = 100, hits = 0}
local limit, lives = 3, 3
function M.limits() return limit .. ' ' .. lives end
function M.die() lives = lives - 1 end
return M
)"}, {"source/main.lua", "tuning = require('tuning') tuning.hits = 5 tuning.die()"}});

    edit({{"source/tuning.lua", R"(
local M = {speed = 150, hits = 1}
local limit, lives = 4, 9
function M.limits() return limit .. ' ' .. lives end
function M.die() lives = lives - 1 end
return M
)"}});
    EXPECT_EQ(lua("return tuning.speed"), "150");
    EXPECT_EQ(lua("return tuning.hits"), "5");
    EXPECT_EQ(lua("return tuning.limits()"), "4 2");
}

TEST_F(ModuleReloadTest, MergesDecodedJsonLikePlainTables) {
    start({{"source/levels.lua", "local json = require('json')\nreturn {waves = json.decode('[1, 2, 3]'), empty = json.array()}\n"}, {"source/main.lua", "levels = require('levels') waves = levels.waves"}});

    // Varn marks decoded arrays with a metatable of its own, which still makes them data that a reload merges in place.
    edit({{"source/levels.lua", "local json = require('json')\nreturn {waves = json.decode('[1, 2, 3, 4]'), empty = json.array({5})}\n"}});
    EXPECT_EQ(lua("return tostring(rawequal(waves, levels.waves)) .. ' ' .. #levels.waves .. ' ' .. levels.empty[1]"), "true 4 5");
    EXPECT_EQ(lua("return require('json').encode(levels.waves)"), "[1,2,3,4]");
}

TEST_F(ModuleReloadTest, RemovesDeletedFunctionsAndKeepsRuntimeFields) {
    start({{"source/shop.lua", R"(
local M = {limit = 3}
function M.old() return 'old' end
function M.buy() return 'buy' end
return M
)"}, {"source/main.lua", "shop = require('shop') shop.cache = {} cache = shop.cache"}});

    edit({{"source/shop.lua", R"(
local M = {}
function M.buy() return 'buy again' end
return M
)"}});
    EXPECT_EQ(lua("return tostring(shop.old) .. ' ' .. tostring(shop.limit)"), "nil nil");
    EXPECT_EQ(lua("return shop.cache == cache"), "true");
    EXPECT_EQ(lua("return shop.buy()"), "buy again");
}

TEST_F(ModuleReloadTest, KeepsEngineObjectsOfModuleFields) {
    start({{"source/world.lua", R"(
local signal = require('haylen.signal')
local M = {hit = signal.new('hit'), version = 1}
function M.describe() return 'v1' end
return M
)"}, {"source/main.lua", "world = require('world') hit = world.hit"}});

    edit({{"source/world.lua", R"(
local signal = require('haylen.signal')
local M = {hit = signal.new('hit'), version = 1}
function M.describe() return 'v2' end
return M
)"}});
    EXPECT_EQ(lua("return world.hit == hit"), "true");
    EXPECT_EQ(lua("return world.describe()"), "v2");
}

TEST_F(ModuleReloadTest, UpdatesInstancesOfClasses) {
    start({{"source/unit.lua", R"(
local haylen = require('haylen')
local Unit = haylen.class('Unit')
function Unit:init(name) self.name = name end
function Unit:describe() return 'v1 ' .. self.name end
function Unit:rest() return 'resting' end
return Unit
)"}, {"source/main.lua", "Unit = require('unit') ana = Unit('Ana') bo = Unit.new('Bo') Unit.count = 2"}});

    edit({{"source/unit.lua", R"(
local haylen = require('haylen')
local Unit = haylen.class('Unit')
function Unit:init(name) self.name = name end
function Unit:describe() return 'v2 ' .. self.name end
return Unit
)"}});
    EXPECT_EQ(lua("return ana:describe() .. ' ' .. bo:describe()"), "v2 Ana v2 Bo");
    EXPECT_EQ(lua("return tostring(ana.rest)"), "nil");
    EXPECT_EQ(lua("return getmetatable(ana) == Unit and require('unit') == Unit"), "true");
    EXPECT_EQ(lua("return getmetatable(Unit.new('Cy')) == Unit and Unit('Di'):describe()"), "v2 Di");
    EXPECT_EQ(lua("return Unit.count"), "2");
}

TEST_F(ModuleReloadTest, UpdatesInheritedMetamethodsAndMixins) {
    start({{"source/unit.lua", R"(
local haylen = require('haylen')
local M = {}
M.Walker = {walk = function(self) return 'walks v1' end}
M.Unit = haylen.class('Unit')
function M.Unit:init(name) self.name = name end
function M.Unit.__eq(a, b) return a.name == b.name end
return M
)"}, {"source/hero.lua", R"(
local haylen = require('haylen')
local unit = require('unit')
local Hero = haylen.class('Hero', unit.Unit, unit.Walker)
return Hero
)"}, {"source/main.lua", "Hero = require('hero') a = Hero('Ana') b = Hero('Ana')"}});
    EXPECT_EQ(lua("return tostring(a == b) .. ' ' .. a:walk()"), "true walks v1");

    edit({{"source/unit.lua", R"(
local haylen = require('haylen')
local M = {}
M.Walker = {walk = function(self) return 'walks v2' end}
M.Unit = haylen.class('Unit')
function M.Unit:init(name) self.name = name end
function M.Unit.__eq(a, b) return false end
return M
)"}});
    EXPECT_EQ(lua("return tostring(a == b) .. ' ' .. a:walk()"), "false walks v2");
}

TEST_F(ModuleReloadTest, SwapsFunctionsCopiedIntoOtherModules) {
    start({{"source/enemies.lua", R"(
local M = {}
function M.spawn() return 'spawn v1' end
function M.tick() ticked = 'tick v1' end
return M
)"}, {"source/waves.lua", R"(
local spawn = require('enemies').spawn
return {next = function() return spawn() end}
)"}, {"source/main.lua", "waves = require('waves') require('haylen.timer').every(0.01, require('enemies').tick)"}});
    fixture->frames(2);
    EXPECT_EQ(lua("return waves.next() .. ' ' .. ticked"), "spawn v1 tick v1");

    edit({{"source/enemies.lua", R"(
local M = {}
function M.spawn() return 'spawn v2' end
function M.tick() ticked = 'tick v2' end
return M
)"}});
    fixture->frames(2);
    EXPECT_EQ(lua("return waves.next() .. ' ' .. ticked"), "spawn v2 tick v2");
}

TEST_F(ModuleReloadTest, KeepsOldBodiesOfRuntimeClosures) {
    start({{"source/level.lua", R"(
local haylen = require('haylen')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local Level = haylen.class('Level', scene.Scene)
function Level:enter()
    timer.every(0.01, function() mark = 'v1' end, {owner = self})
    timer.every(0.01, function() self:tick() end, {owner = self})
end
function Level:tick() ticked = 'tick v1' end
return Level
)"}, {"source/main.lua", "require('haylen.scene').push(require('level')())"}});
    fixture->frames(3);

    edit({{"source/level.lua", R"(
local haylen = require('haylen')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local Level = haylen.class('Level', scene.Scene)
function Level:enter()
    timer.every(0.01, function() mark = 'v2' end, {owner = self})
    timer.every(0.01, function() self:tick() end, {owner = self})
end
function Level:tick() ticked = 'tick v2' end
return Level
)"}});
    fixture->frames(3);
    EXPECT_EQ(lua("return mark .. ' ' .. ticked"), "v1 tick v2");
}

TEST_F(ModuleReloadTest, SharesModuleLocalsWithSuspendedCoroutines) {
    start({{"source/loop.lua", R"(
local async = require('async')
local M = {}
local step = 0
local function advance() step = step + 1 end
function M.step() return step end
function M.run()
    async.spawn(function()
        while true do
            advance()
            async.sleep(10):await()
        end
    end)
end
return M
)"}, {"source/main.lua", "loop = require('loop') loop.run()"}});
    ASSERT_TRUE(fixture->frameUntil([this] { return lua("return loop.step() >= 2") == "true"; }));

    edit({{"source/loop.lua", R"(
local async = require('async')
local M = {}
local step = 0
local function advance() step = step + 100 end
function M.step() return step end
function M.run()
    async.spawn(function()
        while true do
            advance()
            async.sleep(10):await()
        end
    end)
end
return M
)"}});
    ASSERT_TRUE(fixture->frameUntil([this] { return lua("return loop.step() >= 100") == "true"; }));
    EXPECT_EQ(lua("return loop.step() % 100 >= 2"), "true");
}

TEST_F(ModuleReloadTest, ReplacesModuleLevelRegistrations) {
    start({{"source/pings.lua", R"(
local events = require('haylen.events')
pings = 0
events.on('ping', function() pings = pings + 1 end)
return {version = 1}
)"}, {"source/main.lua", "events = require('haylen.events') require('pings')"}});
    fixture->runLua("events.emit('ping')");
    EXPECT_EQ(lua("return pings"), "1");

    edit({{"source/pings.lua", R"(
local events = require('haylen.events')
-- Edited.
pings = 0
events.on('ping', function() pings = pings + 1 end)
return {version = 2}
)"}});
    fixture->runLua("events.emit('ping')");
    EXPECT_EQ(lua("return pings"), "2");
    EXPECT_EQ(lua("return require('pings').version"), "2");
}

TEST_F(ModuleReloadTest, EndsEveryKindOfRegistrationOfTheTopLevel) {
    const std::string module = R"(
local debug = require('haylen.debug')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')
bus:connect(function() end)
timers[#timers + 1] = timer.every(1, function() end)
tween.to(target, 10, {x = 100}, {tag = 'module'})
debug.addMonitor('module', function() return 1 end)
ui.mount(ui.label{text = 'Module'})
return {}
)";
    start({{"source/hud.lua", module}, {"source/main.lua", R"(
local events = require('haylen.events')
bus = require('haylen.signal').new('bus')
timers, target, unmounted = {}, {x = 0}, 0
events.on('guiUnmounted', function() unmounted = unmounted + 1 end)
require('hud')
)"}});
    fixture->frames(2);
    edit({{"source/hud.lua", module + "-- Edited.\n"}});
    fixture->frames(2);
    EXPECT_EQ(lua("return bus.size"), "1");
    EXPECT_EQ(lua("local timer = require('haylen.timer') return tostring(timer.active(timers[1])) .. ' ' .. tostring(timer.active(timers[2]))"), "false true");
    EXPECT_EQ(lua("return #require('haylen.debug').monitors()"), "1");
    EXPECT_EQ(lua("return unmounted"), "1");
}

TEST_F(ModuleReloadTest, HandsKeptValuesToTheNextLoad) {
    const std::string player = R"(
local hotReload = require('haylen.hotReload')
local signal = require('haylen.signal')
local player = hotReload.keep('player', function()
    return {health = 100, healthChanged = signal.new('player.healthChanged')}
end)
player.healthChanged:connect(function(health) heard = (heard or 0) + 1 end)
function player:damage(amount)
    self.health = math.max(self.health - amount, 0)
    self.healthChanged:emit(self.health)
end
return player
)";
    start({{"source/player.lua", player}, {"source/main.lua", "player = require('player') changed = player.healthChanged"}});
    edit({{"source/player.lua", player + "-- Edited.\n"}});
    fixture->runLua("player:damage(10)");
    EXPECT_EQ(lua("return require('player') == player and player.healthChanged == changed"), "true");
    EXPECT_EQ(lua("return player.health .. ' ' .. heard"), "90 1");
}

TEST_F(ModuleReloadTest, MergesGlobalsTheModuleWrites) {
    start({{"source/score.lua", R"(
Score = 0
Bonus = 1
Level = 'one'
function helper() return 'v1' end
return true
)"}, {"source/main.lua", "require('score') Score = 7 Level = 'two'"}});

    // A global that the new code no longer writes goes away, unless the app changed it.
    edit({{"source/score.lua", R"(
Score = 0
function helper() return 'v2' end
return true
)"}});
    EXPECT_EQ(lua("return Score .. ' ' .. helper() .. ' ' .. tostring(Bonus) .. ' ' .. Level"), "7 v2 nil two");
}

TEST_F(ModuleReloadTest, ReloadsModulesThatReturnFunctions) {
    start({{"source/greet.lua", "local greeting = 'Hello' return function(name) return greeting .. ' ' .. name end"}, {"source/main.lua", "greet = require('greet')"}});
    edit({{"source/greet.lua", "local greeting = 'Hi' return function(name) return greeting .. ', ' .. name .. '!' end"}});
    EXPECT_EQ(lua("return greet('Ana') .. ' ' .. tostring(require('greet') == greet)"), "Hi, Ana! true");
    EXPECT_FALSE(restarted());
}

// A module that returns nothing, like a module that writes its own entry of `package.loaded`, loads the way `require` loads it outside development.
TEST_F(ModuleReloadTest, LoadsAndReloadsModulesThatReturnNothing) {
    start({{"source/setup.lua", "configured = 'v1'"}, {"source/own.lua", "package.loaded.own = {name = 'own'}"},
        {"source/main.lua", "package.preload.silent = function() silentRuns = (silentRuns or 0) + 1 end setup, own = require('setup'), require('own')"}});
    EXPECT_EQ(lua("return tostring(setup) .. ' ' .. own.name .. ' ' .. configured"), "true own v1");
    EXPECT_EQ(lua("return tostring(require('silent')) .. ' ' .. tostring(require('silent')) .. ' ' .. silentRuns"), "true true 1");

    edit({{"source/setup.lua", "configured = 'v2'"}});
    EXPECT_EQ(lua("return tostring(require('setup')) .. ' ' .. configured"), "true v2");
    EXPECT_FALSE(restarted());
}

TEST_F(ModuleReloadTest, KeepsUserdataAndCoroutinesOfModuleFields) {
    start({{"source/things.lua", R"(
local M = {point = require('haylen.math').vec2(1, 0), worker = coroutine.create(function() end)}
function M.version() return 1 end
return M
)"}, {"source/main.lua", "things = require('things') point, worker = things.point, things.worker"}});

    edit({{"source/things.lua", R"(
local M = {point = require('haylen.math').vec2(2, 0), worker = coroutine.create(function() end)}
function M.version() return 2 end
return M
)"}});
    EXPECT_EQ(lua("return tostring(rawequal(things.point, point) and things.worker == worker) .. ' ' .. things.point.x .. ' ' .. things.version()"), "true 1.0 2");
}

TEST_F(ModuleReloadTest, KeepsAFieldTheAppChangedWhenTheNewCodeGivesItAnotherKind) {
    start({{"source/modes.lua", "return {mode = 'easy'}"}, {"source/main.lua", "modes = require('modes') modes.mode = 3"}});
    recordLog();
    edit({{"source/modes.lua", "return {mode = {level = 'hard'}}"}});
    EXPECT_EQ(lua("return modes.mode"), "3");
    EXPECT_TRUE(logged("The field \"modes.mode\" of the module \"modes\" keeps its value, because the app changed it and the new code gives it another kind."));
}

// A table that the new code no longer has goes away while it still holds what the last load put in it, however it nests or refers to itself.
TEST_F(ModuleReloadTest, RemovesDroppedTablesUnlessTheAppChangedThem) {
    start({{"source/settings.lua", R"(
local M = {version = function() return 1 end}
M.kept = {volume = 1, nested = {level = 2}}
M.kept.self = M.kept
M.changed = {volume = 1}
M.grown = {volume = 1}
return M
)"}, {"source/main.lua", "settings = require('settings') settings.changed.volume = 5 settings.grown.extra = true"}});

    edit({{"source/settings.lua", "return {version = function() return 2 end}"}});
    EXPECT_EQ(lua("return tostring(settings.kept) .. ' ' .. settings.changed.volume .. ' ' .. tostring(settings.grown.extra) .. ' ' .. settings.version()"), "nil 5 true 2");
}

// Tables keyed by tables of the module, or by tables of other modules, take the edited literals at every reload and keep one entry for every key.
TEST_F(ModuleReloadTest, TakesEditedValuesOfTablesKeyedByTables) {
    const auto kinds = [](const std::string& label) {
        return "local Kind = {}\nlocal Other = require('other')\nreturn {Kind, labels = {[Kind] = '" + label + "', [Other] = '" + label + "'}}";
    };
    start({{"source/other.lua", "return {}"}, {"source/kinds.lua", kinds("v1")}, {"source/main.lua", "kinds, other = require('kinds'), require('other') kind = kinds[1]"}});
    edit({{"source/kinds.lua", kinds("v2")}});
    EXPECT_EQ(lua("return kinds.labels[kind] .. ' ' .. kinds.labels[other]"), "v2 v2");

    edit({{"source/kinds.lua", kinds("v3")}});
    EXPECT_EQ(lua("local count = 0 for _ in pairs(kinds.labels) do count = count + 1 end return kinds.labels[kind] .. ' ' .. kinds.labels[other] .. ' ' .. count .. ' ' .. tostring(kinds[1] == kind)"), "v3 v3 2 true");
}

// A class that the new code gives a parent finds the members of the parent.
TEST_F(ModuleReloadTest, GivesTablesTheMetatableTheirNewCodeSets) {
    start({{"source/classes.lua", R"(
local M = {}
M.Base = {describe = function() return 'base v1' end}
M.Base.__index = M.Base
M.Thing = {}
M.Thing.__index = M.Thing
return M
)"}, {"source/main.lua", "classes = require('classes') thing = setmetatable({}, classes.Thing)"}});
    EXPECT_EQ(lua("return tostring(thing.describe)"), "nil");

    edit({{"source/classes.lua", R"(
local M = {}
M.Base = {describe = function() return 'base v2' end}
M.Base.__index = M.Base
M.Thing = setmetatable({}, M.Base)
M.Thing.__index = M.Thing
return M
)"}});
    EXPECT_EQ(lua("return thing.describe()"), "base v2");
}

// Only Lua functions of the module file take the place of the functions the last load made, so native functions stay as they are everywhere.
TEST_F(ModuleReloadTest, ReplacesOnlyTheFunctionsOfTheModule) {
    start({{"source/tools.lua", "return {format = string.format}"}, {"source/main.lua", "tools = require('tools')"}});
    edit({{"source/tools.lua", "return {format = function() return 'own' end}"}});
    EXPECT_EQ(lua("return tools.format() .. ' ' .. string.format('%d', 7) .. ' ' .. tostring(string.format ~= tools.format)"), "own 7 true");
}

TEST_F(ModuleReloadTest, JoinsTheVariablesThatTheNewCodeAdds) {
    start({{"source/score.lua", R"(
local M = {}
local points = 0
function M.add(amount) points = points + amount return points end
return M
)"}, {"source/main.lua", "score = require('score') score.add(5)"}});

    edit({{"source/score.lua", R"(
local M = {}
local points = 0
local bonus = 10
function M.add(amount) points = points + amount return points end
function M.total() return points + bonus end
return M
)"}});
    EXPECT_EQ(lua("return score.total() .. ' ' .. score.add(1)"), "15 6");
}

// The walk of the heap after a reload reaches the keys of tables, the user values that native code keeps and the locals of suspended coroutines, while a coroutine that failed keeps its stack as it was.
TEST_F(ModuleReloadTest, PutsTheNewFunctionsInKeysUserValuesAndSuspendedCoroutines) {
    start({{"source/counter.lua", "local M = {}\nfunction M.label() return 'v1' end\nreturn M"}, {"source/main.lua", R"(
counter = require('counter')
handlers = {[counter.label] = 'label'}
waiting = coroutine.create(function()
    local label = counter.label
    coroutine.yield()
    resumed = label()
end)
coroutine.resume(waiting)
failed = coroutine.create(function()
    local label = counter.label
    error('failed on purpose')
end)
coroutine.resume(failed)
)"}});
    lua_State* L = fixture->lua();
    lua_newuserdatauv(L, 0, 1);
    lua_getglobal(L, "counter");
    lua_getfield(L, -1, "label");
    lua_setiuservalue(L, -3, 1);
    lua_pop(L, 1);
    lua_setglobal(L, "holder");

    edit({{"source/counter.lua", "local M = {}\nfunction M.label() return 'v2' end\nreturn M"}});
    EXPECT_EQ(lua("return handlers[counter.label]"), "label");
    EXPECT_EQ(lua("coroutine.resume(waiting) return resumed"), "v2");
    EXPECT_EQ(lua("return select(2, debug.getlocal(failed, 1, 1))()"), "v1");

    lua_getglobal(L, "holder");
    lua_getiuservalue(L, -1, 1);
    lua_getglobal(L, "counter");
    lua_getfield(L, -1, "label");
    EXPECT_TRUE(lua_rawequal(L, -1, -3));
    lua_pop(L, 4);
}

TEST_F(ModuleReloadTest, EndsWhatAFailedCreateOfAKeptValueRegistered) {
    start({{"source/radio.lua", R"(
local events = require('haylen.events')
return require('haylen.hotReload').keep('radio', function()
    created = (created or 0) + 1
    events.on('ping', function() heard = (heard or 0) + 1 end)
    if created == 1 then
        error('create failed')
    end
    return {}
end)
)"}, {"source/main.lua", "events = require('haylen.events') ok, message = pcall(require, 'radio')"}});
    fixture->runLua("events.emit('ping')");
    EXPECT_EQ(lua("return tostring(ok) .. ' ' .. tostring(heard)"), "false nil");
    EXPECT_NE(lua("return message").find("create failed"), std::string::npos);

    fixture->runLua("require('radio') events.emit('ping')");
    EXPECT_EQ(lua("return created .. ' ' .. heard"), "2 1");
}

// A loader that a searcher of the app returns for a file of the package loads as it is, whether or not it is the chunk of the file.
TEST_F(ModuleReloadTest, LoadsTheLoadersOfCustomSearchers) {
    start({{"source/bare.lua", "return 'file'"}, {"source/closure.lua", "return 'file'"}, {"source/main.lua", R"(
local marker = 'captured'
table.insert(package.searchers, 1, function(name)
    if name == 'bare' then
        return function() return 'bare' end, 'source/bare.lua'
    end
    if name == 'closure' then
        return function() return marker end, 'source/closure.lua'
    end
end)
)"}});
    EXPECT_EQ(lua("return require('bare') .. ' ' .. require('closure')"), "bare captured");
    EXPECT_EQ(error(), "");
}

TEST_F(ModuleReloadTest, ReloadsChangedModulesInDependencyOrder) {
    start({{"source/a.lua", "local b = require('b') local a = {seen = b.version} function a.f() return 1 end return a"}, {"source/b.lua", "return {version = 1}"}, {"source/main.lua", "a = require('a')"}});
    edit({{"source/a.lua", "local b = require('b') local a = {seen = b.version} function a.f() return 2 end return a"}, {"source/b.lua", "return {version = 2}"}});
    EXPECT_EQ(lua("return a.seen .. ' ' .. a.f()"), "2 2");
}

TEST_F(ModuleReloadTest, CallsReloadedOnModulesDependentsAutoloadsScenesAndInstances) {
    start({{"app.json", R"({"name": "Hooks", "autoload": ["service"]})"},
        {"source/core.lua", R"(
local haylen = require('haylen')
local M = {}
M.Thing = haylen.class('Thing')
function M.Thing:reloaded() order[#order + 1] = 'instance' end
function M.reloaded(self, info) order[#order + 1] = 'core ' .. info.modules[1] .. ' ' .. info.paths[1] end
return M
)"},
        {"source/user.lua", "local core = require('core') return {reloaded = function() order[#order + 1] = 'user' end}"},
        {"source/service.lua", "return {reloaded = function() order[#order + 1] = 'autoload' end}"},
        {"source/main.lua", R"(
order = {}
local scene = require('haylen.scene')
require('user')
thing = require('core').Thing()
scene.push({reloaded = function() order[#order + 1] = 'bottom' end})
scene.push({reloaded = function() order[#order + 1] = 'top' end, transparent = true})
)"}});
    fixture->frames(2);

    edit({{"source/core.lua", R"(
local haylen = require('haylen')
local M = {}
M.Thing = haylen.class('Thing')
function M.Thing:reloaded() order[#order + 1] = 'instance' end
function M.reloaded(self, info) order[#order + 1] = 'core ' .. info.modules[1] .. ' ' .. info.paths[1] end
M.edited = true
return M
)"}});
    EXPECT_EQ(lua("return table.concat(order, ', ')"), "core core source/core.lua, user, autoload, bottom, top, instance");
}

TEST_F(ModuleReloadTest, PublishesModuleReloadedEvents) {
    start({{"source/a.lua", "return {f = function() return 1 end}"}, {"source/b.lua", "return {f = function() return 1 end}"},
        {"source/main.lua", "require('a') require('b') heard = {} require('haylen.events').on('moduleReloaded', function(event) heard[#heard + 1] = event.module .. '@' .. event.path end)"}});
    edit({{"source/a.lua", "return {f = function() return 2 end}"}, {"source/b.lua", "return {f = function() return 2 end}"}});
    EXPECT_EQ(lua("return table.concat(heard, ' ')"), "a@source/a.lua b@source/b.lua");
}

TEST_F(ModuleReloadTest, IgnoresUnchangedSourcesAndModulesNotLoaded) {
    const std::string module = "return {reloaded = function() hooked = true end}";
    start({{"source/same.lua", module}, {"source/main.lua", "require('same')"}});
    edit({{"source/same.lua", module}, {"source/later.lua", "return {value = 'new'}"}});
    EXPECT_EQ(lua("return tostring(hooked)"), "nil");
    EXPECT_FALSE(restarted());
    EXPECT_EQ(lua("return require('later').value"), "new");
}

TEST_F(ModuleReloadTest, KeepsTheOldModuleAfterASyntaxErrorAndResumesOnceFixed) {
    start({{"source/counter.lua", "local M = {} function M.update() return 'v1' end return M"}, {"source/main.lua", "counter = require('counter')"}});
    edit({{"source/counter.lua", "local M = {}\nfunction M.update(\nreturn M"}});
    ASSERT_NE(fixture->engine().getError(), nullptr);
    EXPECT_EQ(fixture->engine().getError()->getFile(), "source/counter.lua");
    EXPECT_EQ(fixture->engine().getError()->getLine(), 3);
    EXPECT_TRUE(fixture->engine().isErrorResumable());
    EXPECT_EQ(lua("return counter.update()"), "v1");

    edit({{"source/counter.lua", "local M = {} function M.update() return 'v2' end return M"}});
    EXPECT_EQ(fixture->engine().getError(), nullptr);
    EXPECT_EQ(lua("return counter.update()"), "v2");
}

TEST_F(ModuleReloadTest, UndoesRegistrationsOfAFailedRun) {
    start({{"source/radio.lua", "local events = require('haylen.events') events.on('ping', function() heard = (heard or 0) + 1 end) return {}"},
        {"source/main.lua", "events = require('haylen.events') require('radio')"}});
    edit({{"source/radio.lua", R"(
local events = require('haylen.events')
local timer = require('haylen.timer')
events.on('ping', function() failed = true end)
timer.after(0, function() failed = true end)
error('broken on purpose')
)"}});
    EXPECT_NE(error().find("broken on purpose"), std::string::npos);
    fixture->runLua("events.emit('ping')");
    fixture->frames(2);
    EXPECT_EQ(lua("return tostring(failed) .. ' ' .. heard"), "nil 1");
}

TEST_F(ModuleReloadTest, ResumesAfterAFixedUpdateError) {
    start({{"source/level.lua", R"(
local Level = {}
Level.__index = Level
function Level.new() return setmetatable({elapsed = 0}, Level) end
function Level:update(dt)
    self.elapsed = self.elapsed + 1
    if self.elapsed > 3 then local broken = nil; return broken.field end
end
return Level
)"}, {"source/main.lua", "level = require('level').new() require('haylen.scene').push(level)"}});
    fixture->frames(5);
    ASSERT_NE(fixture->engine().getError(), nullptr);
    EXPECT_TRUE(fixture->engine().isErrorResumable());

    edit({{"source/level.lua", R"(
local Level = {}
Level.__index = Level
function Level.new() return setmetatable({elapsed = 0}, Level) end
function Level:update(dt)
    self.elapsed = self.elapsed + 1
end
return Level
)"}});
    EXPECT_EQ(fixture->engine().getError(), nullptr);
    fixture->frames(3);
    EXPECT_EQ(lua("return level.elapsed > 6"), "true");
}

TEST_F(ModuleReloadTest, RestartsAfterAFixedLifecycleError) {
    start({{"source/level.lua", "return {enter = function() error('enter failed') end}"}, {"source/main.lua", "require('haylen.scene').push((require('level')))"}});
    fixture->frames(2);
    ASSERT_NE(fixture->engine().getError(), nullptr);
    EXPECT_NE(error().find("enter failed"), std::string::npos);
    EXPECT_FALSE(fixture->engine().isErrorResumable());
    edit({{"source/level.lua", "return {enter = function() end}"}});
    EXPECT_TRUE(restarted());
}

TEST_F(ModuleReloadTest, ResumesOnceAModuleWhoseFirstLoadFailedIsFixed) {
    start({{"source/late.lua", "return {value ="}, {"source/main.lua", "require('haylen.scene').push({update = function() value = require('late').value end})"}});
    fixture->frames(2);
    ASSERT_NE(fixture->engine().getError(), nullptr);
    EXPECT_EQ(fixture->engine().getError()->getFile(), "source/late.lua");
    EXPECT_TRUE(fixture->engine().isErrorResumable());

    edit({{"source/late.lua", "return {value = 'fixed'}"}});
    EXPECT_EQ(fixture->engine().getError(), nullptr);
    fixture->frames(1);
    EXPECT_EQ(lua("return value"), "fixed");
    EXPECT_FALSE(restarted());
}

TEST_F(ModuleReloadTest, RestartsOnceAModuleWhoseFirstLoadFailedInALifecycleStepIsFixed) {
    start({{"source/late.lua", "return {value ="}, {"source/main.lua", "require('haylen.scene').push({enter = function() require('late') end})"}});
    fixture->frames(2);
    ASSERT_NE(fixture->engine().getError(), nullptr);
    EXPECT_FALSE(fixture->engine().isErrorResumable());
    edit({{"source/late.lua", "return {value = 'fixed'}"}});
    EXPECT_TRUE(restarted());
}

TEST_F(ModuleReloadTest, RestartsForAppJsonMainAndPluginManifests) {
    for (const std::string path : {"app.json", "source/main.lua", "plugins/ads/plugin.json"}) {
        start({{"source/main.lua", ""}});
        recordLog();
        edit({{path, "{}"}});
        EXPECT_TRUE(restarted()) << path;
        EXPECT_TRUE(logged("The file \"" + path + "\" changed, restarting the app.")) << path;
        core::Log::removeListener(listener);
    }
}

TEST_F(ModuleReloadTest, RestartsInTheRestartModeAndForMarkedModules) {
    start({{"app.json", R"({"name": "Restart", "debug": {"reload": "restart"}})"}, {"source/a.lua", "return {}"}, {"source/main.lua", "require('a')"}});
    EXPECT_EQ(lua("return require('haylen.hotReload').mode()"), "restart");
    edit({{"source/a.lua", "return {edited = true}"}});
    EXPECT_TRUE(restarted());

    start({{"source/marked.lua", "require('haylen.hotReload').restartOnChange() return {}"}, {"source/other.lua", "return {}"}, {"source/main.lua", "require('marked') require('other')"}});
    edit({{"source/other.lua", "return {edited = true}"}});
    EXPECT_FALSE(restarted());
    edit({{"source/marked.lua", "require('haylen.hotReload').restartOnChange() return {edited = true}"}});
    EXPECT_TRUE(restarted());
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"debug": {"reload": "always"}})")), std::invalid_argument);
}

TEST_F(ModuleReloadTest, RestartsForAmbiguousUpvalues) {
    const std::string module = R"(
local M = {}
local x = 1
function M.first() return x end
do
    local x = 2
    function M.second() return x end
end
return M
)";
    start({{"source/twins.lua", module}, {"source/main.lua", "require('twins')"}});
    recordLog();
    edit({{"source/twins.lua", module + "-- Edited.\n"}});
    EXPECT_TRUE(restarted());
    EXPECT_TRUE(logged("The module \"twins\" has two different variables named \"x\" that its functions capture."));
}

TEST_F(ModuleReloadTest, RestartsWhenTheReturnKindChanges) {
    start({{"source/kind.lua", "return {}"}, {"source/main.lua", "require('kind')"}});
    recordLog();
    edit({{"source/kind.lua", "return function() end"}});
    EXPECT_TRUE(restarted());
    EXPECT_TRUE(logged("The module \"kind\" now returns a function instead of a table."));
}

TEST_F(ModuleReloadTest, RestartsForRemovedModules) {
    start({{"source/gone.lua", "return {}"}, {"source/main.lua", "require('gone')"}});
    remove("source/gone.lua");
    EXPECT_TRUE(restarted());
}

TEST_F(ModuleReloadTest, AddsTheSameAutoloadAgain) {
    start({{"source/music.lua", "return {volume = 1}"}, {"source/setup.lua", "require('haylen').autoload('music', 'music') return {f = function() return 1 end}"},
        {"source/main.lua", "require('setup') music = require('haylen').autoloads.music"}});
    edit({{"source/setup.lua", "require('haylen').autoload('music', 'music') return {f = function() return 2 end}"}});
    EXPECT_EQ(error(), "");
    EXPECT_EQ(lua("return require('haylen').autoloads.music == music and require('setup').f()"), "2");
    EXPECT_NE(lua("require('haylen').autoload('music', 'setup')").find("An autoload named \"music\" already exists."), std::string::npos);
}

TEST_F(ModuleReloadTest, KeepsWhatAnAutoloadStartedWhenTheModuleThatAddedItReloads) {
    start({{"source/music.lua", "return {start = function(self) require('haylen.events').on('ping', function() self.heard = (self.heard or 0) + 1 end) end}"},
        {"source/setup.lua", "require('haylen').autoload('music', 'music') return {}"},
        {"source/main.lua", "events = require('haylen.events') require('setup') music = require('music')"}});
    edit({{"source/setup.lua", "require('haylen').autoload('music', 'music') return {edited = true}"}});
    fixture->runLua("events.emit('ping')");
    EXPECT_EQ(lua("return music.heard"), "1");
}

TEST_F(ModuleReloadTest, KeepsWhatTheCreateOfAKeptValueRegistered) {
    const std::string radio = R"(
local events = require('haylen.events')
return require('haylen.hotReload').keep('radio', function()
    local radio = {heard = 0}
    events.on('ping', function() radio.heard = radio.heard + 1 end)
    return radio
end)
)";
    start({{"source/radio.lua", radio}, {"source/main.lua", "events = require('haylen.events') radio = require('radio')"}});
    edit({{"source/radio.lua", radio + "-- Edited.\n"}});
    fixture->runLua("events.emit('ping')");
    EXPECT_EQ(lua("return radio.heard"), "1");
}

TEST_F(ModuleReloadTest, DropsTheValuesThatAFailedLoadKept) {
    const std::string radio = R"(
local events = require('haylen.events')
local radio = require('haylen.hotReload').keep('radio', function()
    created = (created or 0) + 1
    events.on('ping', function() heard = (heard or 0) + 1 end)
    return {}
end)
)";
    start({{"source/radio.lua", radio + "error('broken on purpose')"}, {"source/main.lua", "events = require('haylen.events') pcall(require, 'radio')"}});
    fixture->runLua("events.emit('ping')");
    EXPECT_EQ(lua("return tostring(heard)"), "nil");

    fixture->package().setFile("source/radio.lua", test::TestFiles::bytes(radio + "return radio"));
    fixture->runLua("require('radio') events.emit('ping')");
    EXPECT_EQ(lua("return created .. ' ' .. heard"), "2 1");
}

TEST_F(ModuleReloadTest, ShowsErrorsOfReloadedHooksUntilAFixReloads) {
    start({{"source/hooked.lua", "return {reloaded = function() error('hook failed') end}"}, {"source/main.lua", "require('hooked')"}});
    edit({{"source/hooked.lua", "return {reloaded = function() error('hook failed') end, version = 2}"}});
    EXPECT_NE(error().find("hook failed"), std::string::npos);
    EXPECT_TRUE(fixture->engine().isErrorResumable());

    edit({{"source/hooked.lua", "return {reloaded = function() end, version = 3}"}});
    EXPECT_EQ(error(), "");
}

TEST_F(ModuleReloadTest, CallsTheReloadedHookOfAClassOnItsInstancesOnly) {
    const std::string level = R"(
local haylen = require('haylen')
local Level = haylen.class('Level')
function Level:reloaded() calls = (calls or '') .. (self == Level and 'class ' or 'instance ') end
return Level
)";
    start({{"source/level.lua", level}, {"source/main.lua", "level = require('level')()"}});
    edit({{"source/level.lua", level + "-- Edited.\n"}});
    EXPECT_EQ(lua("return calls"), "instance ");
    EXPECT_EQ(error(), "");
}

TEST_F(ModuleReloadTest, ReloadsTheOtherModulesOfASaveWithAFailedModule) {
    start({{"source/a.lua", "return {version = 1}"}, {"source/b.lua", "return {version = 1}"}, {"source/main.lua", "a = require('a') b = require('b')"}});
    edit({{"source/a.lua", "return {version ="}, {"source/b.lua", "return {version = 2}"}});
    EXPECT_NE(error().find("source/a.lua"), std::string::npos);
    EXPECT_EQ(lua("return a.version .. ' ' .. b.version"), "1 2");

    edit({{"source/a.lua", "return {version = 2}"}});
    EXPECT_EQ(error(), "");
    EXPECT_EQ(lua("return a.version .. ' ' .. b.version"), "2 2");
}

// The tables nest deeper than the bounds of the snapshot and of the functions a reload collects.
TEST_F(ModuleReloadTest, MergesDeeplyNestedTables) {
    const std::string nested = "local root = {} local level = root for depth = 1, 70 do level.next = {depth = depth} level = level.next end level.leaf = ";
    start({{"source/deep.lua", nested + "1 return root"}, {"source/main.lua", "deep = require('deep')"}});
    edit({{"source/deep.lua", nested + "2 return root"}});
    EXPECT_EQ(error(), "");
    EXPECT_EQ(lua("local level = deep for depth = 1, 70 do level = level.next end return level.leaf"), "2");
}

TEST_F(ModuleReloadTest, ReloadsASaveMadeWhileTheModuleFirstLoads) {
    start({{"source/slow.lua", "save() return {version = 1}"}});
    lua_State* L = fixture->lua();
    lua_pushlightuserdata(L, &fixture->package());
    lua_pushcclosure(L, [](lua_State* state) {
        static_cast<io::MemoryPackage*>(lua_touserdata(state, lua_upvalueindex(1)))->setFile("source/slow.lua", test::TestFiles::bytes("save() return {version = 2}"));
        return 0;
    }, 1);
    lua_setglobal(L, "save");
    fixture->runLua("slow = require('slow')");

    const std::vector<std::string> changed{"source/slow.lua"};
    fixture->host().getDevelopmentSession()->addChanges(changed);
    fixture->frames(1);
    EXPECT_EQ(lua("return slow.version"), "2");
}

TEST_F(ModuleReloadTest, ReportsModulesThatCannotLoad) {
    start({{"source/main.lua", R"(
table.insert(package.searchers, 1, function(name)
    if name == 'ghost' then
        return function() return {} end, 'source/ghost.lua'
    end
end)
)"}});
    EXPECT_NE(lua("require('missing')").find("The module \"missing\" was not found."), std::string::npos);
    EXPECT_EQ(lua("local ok, message = pcall(require, 'ghost') return tostring(ok) .. ' ' .. message .. ' ' .. tostring(package.loaded.ghost)"), "false The package file \"test/source/ghost.lua\" was not found. nil");
    EXPECT_EQ(lua("local searchers = package.searchers package.searchers = nil local ok, message = pcall(require, 'missing') package.searchers = searchers return message"), "The field \"package.searchers\" must be a table.");
}

TEST_F(HotReloadLuaTest, ReportsTheModeAndRejectsCallsOutsideAModuleLoad) {
    start({{"source/main.lua", "hotReload = require('haylen.hotReload')"}});
    EXPECT_EQ(lua("return hotReload.active()"), "true");
    EXPECT_EQ(lua("return hotReload.mode()"), "module");
    EXPECT_EQ(lua("return select(2, pcall(hotReload.keep, 'a', function() return 1 end))"), "The function \"keep\" works only while a module loads.");
    EXPECT_EQ(lua("return select(2, pcall(hotReload.restartOnChange))"), "The function \"restartOnChange\" works only while a module loads.");
    EXPECT_NE(lua("hotReload.keep({}, function() end)").find("bad argument #1 to 'keep' (string expected, got table)"), std::string::npos);
    EXPECT_NE(lua("hotReload.keep(1, function() end)").find("bad argument #1 to 'keep' (string expected, got number)"), std::string::npos);

    fixture.reset();
    test::EngineFixture shipped({{"source/main.lua", "hotReload = require('haylen.hotReload')"}});
    EXPECT_EQ(shipped.lua("return hotReload.active()"), "false");
    EXPECT_EQ(shipped.lua("return hotReload.keep('a', function() return {} end) ~= hotReload.keep('a', function() return {} end)"), "true");
    EXPECT_EQ(shipped.lua("hotReload.restartOnChange() return 'ignored'"), "ignored");
}
// clang-format on

} // namespace haylen::lua
