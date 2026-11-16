#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Error.hpp"
#include "platform/DevelopmentSession.hpp"
#include "support/EngineFixture.hpp"
#include "support/EventLoop.hpp"
#include "support/TestFiles.hpp"

namespace haylen::debug {

// Runs apps in development with a short execution budget, whose code never ends in every way a script can try to outlast it.
class ExecutionBudgetTest : public ::testing::Test {
  protected:
    static constexpr const char* kAppJson = R"({"name": "Budget", "identifier": "com.haylen.budget", "debug": {"executionBudget": 0.1}})";

    void start(const std::string& main, std::map<std::string, std::string> files = {}) {
        files["haylen.json"] = kAppJson;
        files["source/main.lua"] = main;
        fixture.reset();
        fixture = std::make_unique<test::EngineFixture>(std::move(files), nullptr, test::EngineFixture::Options{.development = true});
    }

    // The frames that a cancel may take: the code that runs too long or takes too much stops inside the frame it runs in, and a task needs a few turns of the event loop to reach its code. Counting frames instead of time keeps the check apart from how fast the machine runs, such as under a sanitizer.
    static constexpr int kCancelFrames = 10;

    // Runs frames until the app stops, and fails when no frame of the bound stopped it.
    void runUntilCancelled() {
        for (int frame = 0; frame < kCancelFrames && !fixture->engine().isCancelled(); ++frame) {
            fixture->frames(1);
            test::EventLoop::yield();
        }
        ASSERT_TRUE(fixture->engine().isCancelled());
    }

    [[nodiscard]] const lua::Error& error() const {
        return *fixture->engine().getError();
    }

    std::unique_ptr<test::EngineFixture> fixture;
};

TEST_F(ExecutionBudgetTest, CancelsAnUpdateThatNeverEnds) {
    start(R"(
local scene = require('haylen.scene')
scene.push({update = function()
    local count = 0
    while true do
        count = count + 1
    end
end})
)");
    runUntilCancelled();
    EXPECT_EQ(error().getCode(), lua::Error::Code::ExecutionBudget);
    EXPECT_EQ(error().getCallback(), "update");
    EXPECT_EQ(error().getFile(), "source/main.lua");
    EXPECT_GE(error().getLine(), 5);
    EXPECT_LE(error().getLine(), 7);
    EXPECT_NE(error().getMessage().find("ran for more than 100 ms in \"update\""), std::string::npos);
    EXPECT_FALSE(error().getFrames().empty());
    EXPECT_FALSE(fixture->engine().isErrorResumable());
    EXPECT_FALSE(fixture->engine().clearError());

    const core::Json report = error().toJson();
    EXPECT_EQ(report.at("code"), "executionBudget");
    EXPECT_EQ(report.at("callback"), "update");
}

// Every instruction raises the error once the budget ran out, so code that catches it raises it again before it can loop.
TEST_F(ExecutionBudgetTest, CannotBeCaughtByProtectedCallsOrCoroutines) {
    const std::vector<std::string> attempts{
        "while true do pcall(function() while true do end end) end", "while true do pcall(pcall, function() while true do end end) end", "while true do xpcall(function() while true do end end, function() while true do end end) end", "while true do coroutine.resume(coroutine.create(function() while true do end end)) end", "while true do pcall(coroutine.wrap(function() while true do pcall(error, 'again') end end)) end", "local co = coroutine.create(function() while true do coroutine.yield() end end) while true do coroutine.resume(co) end", "while true do local ok = pcall(function() local guard <close> = setmetatable({}, {__close = function() while true do end end}) while true do end end) end", "while true do pcall(string.gsub, string.rep('a', 64), 'a', function() while true do end end) end", "while true do pcall(table.sort, {3, 2, 1}, function() while true do end end) end", "while true do pcall(load('while true do end')) end",
    };
    for (const std::string& attempt : attempts) {
        SCOPED_TRACE(attempt);
        start("require('haylen.scene').push({update = function() " + attempt + " end})");
        runUntilCancelled();
        EXPECT_EQ(error().getCode(), lua::Error::Code::ExecutionBudget);
    }
}

TEST_F(ExecutionBudgetTest, NamesTheStepThatRanTooLong) {
    const std::vector<std::pair<std::string, std::string>> cases{
        {"while true do end", "start"}, {"require('looping')", "start"}, {"require('haylen.timer').after(0, function() while true do end end)", "update"}, {"require('haylen.scene').push({fixedUpdate = function() while true do end end})", "fixedUpdate"}, {"require('haylen.scene').push({render = function() while true do end end})", "render"}, {"local events = require('haylen.events') events.on('ping', function() while true do end end) require('haylen.timer').after(0, function() events.emit('ping') end)", "update"}, {"local async = require('async') async.spawn(function() async.sleep(0):await() while true do end end)", "scripts"},
    };
    for (const auto& [main, stage] : cases) {
        SCOPED_TRACE(main);
        start(main, {{"source/looping.lua", "while true do end"}});
        runUntilCancelled();
        EXPECT_EQ(error().getCallback(), stage);
    }
}

TEST_F(ExecutionBudgetTest, CancelsAReloadThatNeverEnds) {
    start("settings = require('settings')", {{"source/settings.lua", "return {speed = 1}"}});
    fixture->frames(1);
    fixture->package().setFile("source/settings.lua", test::TestFiles::bytes("while true do end return {speed = 2}"));
    const std::vector<std::string> paths{"source/settings.lua"};
    fixture->host().getDevelopmentSession()->addChanges(paths);
    runUntilCancelled();
    EXPECT_EQ(error().getCallback(), "reload");
    EXPECT_EQ(error().getFile(), "source/settings.lua");
}

// A cancelled app shows its error whatever showed before, no fix resumes it, and the next start runs the fixed code with a whole budget.
TEST_F(ExecutionBudgetTest, ReplacesAResumableErrorAndRunsAgainAfterARestart) {
    start("require('haylen.scene').push({update = function() error('first') end}) require('haylen.timer').every(0.01, function() if broken then while true do end end end)");
    fixture->frames(2);
    ASSERT_NE(fixture->engine().getError(), nullptr);
    EXPECT_TRUE(fixture->engine().isErrorResumable());

    fixture->runLua("broken = true");
    fixture->engine().clearError();
    runUntilCancelled();
    EXPECT_EQ(error().getCode(), lua::Error::Code::ExecutionBudget);
    fixture->engine().recover();
    fixture->frames(1);
    EXPECT_TRUE(fixture->engine().isCancelled());

    fixture->package().setFile("source/main.lua", test::TestFiles::bytes("count = 0 require('haylen.scene').push({update = function() count = count + 1 end})"));
    fixture->restart();
    fixture->frames(3);
    EXPECT_EQ(fixture->engine().getError(), nullptr);
    EXPECT_EQ(fixture->lua("return count"), "3");
}

// The memory that the app takes after it caught the error of the budget never counts as room that a collection made, so the budget runs out the first time, also when the app allocates right after the catch and never runs out of memory again.
TEST_F(ExecutionBudgetTest, CancelsAnAppThatTakesMoreMemoryThanItsBudget) {
    const std::vector<std::string> attempts{
        "local kept = {} while true do kept[#kept + 1] = string.rep('x', 4096) .. #kept end",
        "while true do pcall(function() local kept = {} while true do kept[#kept + 1] = string.rep('y', 4096) .. #kept end end) end",
        "if not filled then filled = true pcall(function() local kept = {} while true do kept[#kept + 1] = string.rep('z', 4096) .. #kept end end) local after = {} end",
    };
    for (const std::string& attempt : attempts) {
        SCOPED_TRACE(attempt);
        std::map<std::string, std::string> files{{"haylen.json", R"({"name": "Budget", "identifier": "com.haylen.budget", "debug": {"executionBudget": 30, "memoryBudget": 32}})"}, {"source/main.lua", "require('haylen.scene').push({update = function() " + attempt + " end})"}};
        fixture.reset();
        fixture = std::make_unique<test::EngineFixture>(std::move(files), nullptr, test::EngineFixture::Options{.development = true});
        runUntilCancelled();
        EXPECT_EQ(error().getCode(), lua::Error::Code::Memory);
        EXPECT_NE(error().getMessage().find("took more than 32 MB of Lua memory in \"update\""), std::string::npos);
    }
}

// Garbage that a full collection frees never runs the budget out, however much the app allocated in total.
TEST_F(ExecutionBudgetTest, CountsOnlyTheMemoryThatStaysAfterACollection) {
    std::map<std::string, std::string> files{{"haylen.json", R"({"name": "Budget", "identifier": "com.haylen.budget", "debug": {"memoryBudget": 32}})"}, {"source/main.lua", "for round = 1, 20 do local scratch = {} for index = 1, 50000 do scratch[index] = {index} end end done = true"}};
    fixture = std::make_unique<test::EngineFixture>(std::move(files), nullptr, test::EngineFixture::Options{.development = true});
    EXPECT_EQ(fixture->engine().getError(), nullptr);
    EXPECT_EQ(fixture->lua("return done"), "true");
}

// The `xpcall` of development hands the handler of the app every other error and lets the function it calls yield.
TEST_F(ExecutionBudgetTest, KeepsTheHandlersAndYieldsOfXpcall) {
    start("");
    EXPECT_EQ(fixture->lua("local run = coroutine.wrap(function() return xpcall(function() return coroutine.yield('paused') .. '!' end, debug.traceback) end) local first = run() local ok, value = run('resumed') return first .. ' ' .. tostring(ok) .. ' ' .. value"), "paused true resumed!");
    EXPECT_EQ(fixture->lua("local ok, message = xpcall(error, function(failure) return 'handled ' .. failure end, 'failure', 0) return message"), "handled failure");
}

TEST_F(ExecutionBudgetTest, KeepsTheHooksOfLuaFromTheApp) {
    start("");
    EXPECT_NE(fixture->lua("debug.sethook()").find("The function \"debug.sethook\" is not available in development"), std::string::npos);

    fixture.reset();
    test::EngineFixture shipped({{"source/main.lua", "while false do end"}});
    EXPECT_EQ(shipped.lua("return tostring(debug.gethook())"), "nil");
    EXPECT_EQ(shipped.lua("debug.sethook() return 'free'"), "free");
}

TEST_F(ExecutionBudgetTest, LeavesCodeOutsideTheCallsOfTheHostUnbounded) {
    start("");
    EXPECT_EQ(fixture->lua("local deadline = os.clock() + 0.3 while os.clock() < deadline do end return 'done'"), "done");
    EXPECT_FALSE(fixture->engine().isCancelled());
}

} // namespace haylen::debug
