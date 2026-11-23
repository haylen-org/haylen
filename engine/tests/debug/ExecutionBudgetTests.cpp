#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "debug/ExecutionBudget.hpp"
#include "haylen/core/AppError.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/platform/DevelopmentSession.hpp"
#include "support/DebugClient.hpp"
#include "support/EngineFixture.hpp"
#include "support/EventLoop.hpp"
#include "support/TestFiles.hpp"

namespace haylen::debug {

// Runs apps in development with a short execution budget, whose code never ends in every way a script can try to outlast it.
class ExecutionBudgetTest : public ::testing::Test {
  protected:
    static constexpr const char* kAppJson = R"({"name": "Budget", "identifier": "com.haylen.budget", "lua": {"executionBudget": 0.1}})";

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

    [[nodiscard]] const core::AppError& error() const {
        const core::AppError* stopped = fixture->engine().getError();
        if (stopped == nullptr) {
            throw std::runtime_error("The app holds no error.");
        }

        return *stopped;
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
    EXPECT_EQ(error().getCode(), core::AppError::Code::ExecutionBudget);
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
        EXPECT_EQ(error().getCode(), core::AppError::Code::ExecutionBudget);
    }
}

// Lua runs a finalizer with the hooks of its thread off, so the budget runs it in a coroutine of its own, whatever set the metatable and whatever triggers the collection.
TEST_F(ExecutionBudgetTest, CancelsAFinalizerThatNeverEnds) {
    const std::vector<std::string> attempts{
        "setmetatable({}, {__gc = function() while true do end end}) collectgarbage()",
        "debug.setmetatable({}, {__gc = function() while true do end end}) collectgarbage()",
        "local finalizer = {__gc = function() local count = 0 while true do count = count + 1 end end} for index = 1, 100000 do setmetatable({}, finalizer) end",
    };
    for (const std::string& attempt : attempts) {
        SCOPED_TRACE(attempt);
        start("require('haylen.scene').push({update = function() " + attempt + " end})");
        runUntilCancelled();
        EXPECT_EQ(error().getCode(), core::AppError::Code::ExecutionBudget);
        EXPECT_EQ(error().getCallback(), "update");
        EXPECT_EQ(error().getFile(), "source/main.lua");
    }
}

// The guarded finalizer runs the one of the app with its object, keeps a finalizer that is no function, and reports an error of the finalizer as Lua does.
TEST_F(ExecutionBudgetTest, KeepsWhatFinalizersDo) {
    start("");
    EXPECT_EQ(fixture->lua("local finalizer = {__gc = function(object) collected = (collected or '') .. object.name end} setmetatable({name = 'first'}, finalizer) setmetatable({name = 'second'}, finalizer) collectgarbage() collectgarbage() return collected"), "secondfirst");
    EXPECT_EQ(fixture->lua("local marker = {__gc = true} setmetatable({}, marker) return tostring(marker.__gc)"), "true");

    std::vector<std::string> lines;
    const std::uint64_t listener = core::Log::addListener([&lines](core::Log::Level, std::string_view line) { lines.emplace_back(line); });
    fixture->lua("setmetatable({}, {__gc = function() error('broken finalizer') end}) collectgarbage()");
    core::Log::removeListener(listener);
    EXPECT_TRUE(std::ranges::any_of(lines, [](const std::string& line) { return line.find("broken finalizer") != std::string::npos; })) << ::testing::PrintToString(lines);
    EXPECT_FALSE(fixture->engine().isCancelled());
}

TEST_F(ExecutionBudgetTest, NamesTheStepThatRanTooLong) {
    const std::vector<std::pair<std::string, std::string>> cases{
        {"while true do end", "start"}, {"require('looping')", "start"}, {"require('haylen.timer').after(0, function() while true do end end)", "update"}, {"require('haylen.scene').push({fixedUpdate = function() while true do end end})", "fixedUpdate"}, {"require('haylen.scene').push({render = function() while true do end end})", "render"}, {"local events = require('haylen.events') events.on('ping', function() while true do end end) require('haylen.timer').after(0, function() events.emit('ping') end)", "update"}, {"local async = require('async') async.spawn(function() async.sleep(0):await() while true do end end)", "scripts"}, {"require('haylen.scene').push({update = function() require('haylen.debug').profile('findPath', function() while true do end end) end})", "update"},
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
    fixture->developmentSession().addChanges(paths);
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
    EXPECT_EQ(error().getCode(), core::AppError::Code::ExecutionBudget);
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
        std::map<std::string, std::string> files{{"haylen.json", R"({"name": "Budget", "identifier": "com.haylen.budget", "lua": {"executionBudget": 30, "memoryBudget": 32}})"}, {"source/main.lua", "require('haylen.scene').push({update = function() " + attempt + " end})"}};
        fixture.reset();
        fixture = std::make_unique<test::EngineFixture>(std::move(files), nullptr, test::EngineFixture::Options{.development = true});
        runUntilCancelled();
        EXPECT_EQ(error().getCode(), core::AppError::Code::Memory);
        EXPECT_NE(error().getMessage().find("took more than 32 MB of Lua memory in \"update\""), std::string::npos);
    }
}

// The code that stops an app whose memory ran out runs with its whole time and room for a share of the memory budget, so the unload of its scene ends what it holds.
TEST_F(ExecutionBudgetTest, LetsTheStopOfAnAppWhoseMemoryRanOutRun) {
    std::map<std::string, std::string> files{{"haylen.json", R"({"name": "Budget", "identifier": "com.haylen.budget", "lua": {"executionBudget": 30, "memoryBudget": 32}})"}, {"source/main.lua", "kept = {} require('haylen.scene').push({update = function() while true do kept[#kept + 1] = string.rep('x', 4096) .. #kept end end, unload = function() local made = {} for index = 1, 2000 do made[index] = string.rep('u', 512) .. index end print('Unloaded with ' .. #made .. ' texts.') end})"}};
    fixture = std::make_unique<test::EngineFixture>(std::move(files), nullptr, test::EngineFixture::Options{.development = true});
    runUntilCancelled();
    ASSERT_EQ(error().getCode(), core::AppError::Code::Memory);

    std::vector<std::string> lines;
    const std::uint64_t listener = core::Log::addListener([&lines](core::Log::Level, std::string_view line) { lines.emplace_back(line); });
    fixture.reset();
    core::Log::removeListener(listener);
    EXPECT_TRUE(std::ranges::any_of(lines, [](const std::string& line) { return line.find("Unloaded with 2000 texts.") != std::string::npos; })) << ::testing::PrintToString(lines);
}

// Garbage that a full collection frees never runs the budget out, however much the app allocated in total.
TEST_F(ExecutionBudgetTest, CountsOnlyTheMemoryThatStaysAfterACollection) {
    std::map<std::string, std::string> files{{"haylen.json", R"({"name": "Budget", "identifier": "com.haylen.budget", "lua": {"memoryBudget": 32}})"}, {"source/main.lua", "for round = 1, 20 do local scratch = {} for index = 1, 50000 do scratch[index] = {index} end end done = true"}};
    fixture = std::make_unique<test::EngineFixture>(std::move(files), nullptr, test::EngineFixture::Options{.development = true});
    EXPECT_EQ(fixture->engine().getError(), nullptr);
    EXPECT_EQ(fixture->lua("return done"), "true");
}

// The `xpcall` of development hands the handler of the app every other error and lets the function it calls yield.
// A page cannot halt, so a breakpoint there never gives the code that calls it its whole budget again.
TEST_F(ExecutionBudgetTest, CancelsCodeThatCallsBreakpointsWhereTheAppCannotHalt) {
    start(R"(
local scene = require('haylen.scene')
scene.push({update = function()
    local started = os.clock()
    while os.clock() - started < 2 do
        require('haylen.debug').breakpoint()
    end
end})
)");
    fixture->host().setHaltable(false);
    test::DebugClient client{fixture->host()};
    client.connect(fixture->engine().getPlugin<lua::ScriptPlugin>().getState());
    runUntilCancelled();
    EXPECT_EQ(error().getCode(), core::AppError::Code::ExecutionBudget);
}

// The budget waits while the app halts, so an evaluation that resumes a coroutine of the app after a halt longer than the budget finds the budget whole. The evaluation runs only enough instructions of the app for the budget to look at its clock many times, so it ends far inside the time limit of evaluations on any build, such as under a sanitizer.
TEST_F(ExecutionBudgetTest, WaitsWhileTheDebuggerHaltsTheApp) {
    start(R"(
numbers = coroutine.wrap(function() while true do coroutine.yield(1) end end)
local scene = require('haylen.scene')
scene.push({update = function()
    if not halted then
        halted = true
        require('haylen.debug').breakpoint()
    end
end})
)");
    test::DebugClient client{fixture->host()};
    client.connect(fixture->engine().getPlugin<lua::ScriptPlugin>().getState());
    const int resumes = ExecutionBudget::kCheckInstructions * 5;
    const std::string expression = "(function() local sum = 0 for _ = 1, " + std::to_string(resumes) + " do sum = sum + numbers() end return sum end)()";
    int evaluation = 0;
    // clang-format off
    client.then([&evaluation, &expression](test::DebugClient& debug) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        evaluation = debug.request("evaluate", {{"expression", expression}, {"frameId", 1}});
    });
    // clang-format on
    client.then([](test::DebugClient& debug) { debug.request("continue"); });
    fixture->frames(3);

    EXPECT_FALSE(fixture->engine().isCancelled());
    ASSERT_NE(evaluation, 0);
    const core::Json* response = client.findResponse(evaluation);
    ASSERT_NE(response, nullptr);
    ASSERT_TRUE(response->value("success", false)) << response->dump();
    ASSERT_TRUE(response->contains("body") && response->at("body").contains("result")) << response->dump();
    EXPECT_EQ(response->at("body").at("result"), std::to_string(resumes));
}

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
