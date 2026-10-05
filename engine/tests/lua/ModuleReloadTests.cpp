#include <gtest/gtest.h>

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
function helper() return 'v1' end
return true
)"}, {"source/main.lua", "require('score') Score = 7"}});

    edit({{"source/score.lua", R"(
Score = 0
function helper() return 'v2' end
return true
)"}});
    EXPECT_EQ(lua("return Score .. ' ' .. helper()"), "7 v2");
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
    start({{"source/level.lua", "return {enter = function() error('enter failed') end}"}, {"source/main.lua", "require('haylen.scene').push(require('level'))"}});
    fixture->frames(2);
    ASSERT_NE(fixture->engine().getError(), nullptr);
    EXPECT_FALSE(fixture->engine().isErrorResumable());
    edit({{"source/level.lua", "return {enter = function() end}"}});
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

TEST_F(HotReloadLuaTest, ReportsTheModeAndRejectsCallsOutsideAModuleLoad) {
    start({{"source/main.lua", "hotReload = require('haylen.hotReload')"}});
    EXPECT_EQ(lua("return hotReload.active()"), "true");
    EXPECT_EQ(lua("return hotReload.mode()"), "module");
    EXPECT_EQ(lua("return select(2, pcall(hotReload.keep, 'a', function() return 1 end))"), "The function \"keep\" works only while a module loads.");
    EXPECT_EQ(lua("return select(2, pcall(hotReload.restartOnChange))"), "The function \"restartOnChange\" works only while a module loads.");
    EXPECT_NE(lua("hotReload.keep({}, function() end)").find("bad argument #1 to 'keep' (string expected, got table)"), std::string::npos);

    fixture.reset();
    test::EngineFixture shipped({{"source/main.lua", "hotReload = require('haylen.hotReload')"}});
    EXPECT_EQ(shipped.lua("return hotReload.active()"), "false");
    EXPECT_EQ(shipped.lua("return hotReload.keep('a', function() return {} end) ~= hotReload.keep('a', function() return {} end)"), "true");
}
// clang-format on

} // namespace haylen::lua
