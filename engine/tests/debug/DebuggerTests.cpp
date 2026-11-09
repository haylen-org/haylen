#include <gtest/gtest.h>

#include <lua.hpp>

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include "debug/Debugger.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/lua/Runtime.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/DebugClient.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::debug {

// Drives the debugger with the requests of the protocol, as an editor does, over a headless engine whose code runs through the protected calls of the engine, as its callbacks do.
class DebuggerTest : public ::testing::Test {
  protected:
    static constexpr const char* kModule = R"(local M = {}
function M.add(a, b)
    local sum = a + b
    local info = {name = "point", values = {1, 2}}
    return sum, info
end
function M.run()
    local total = M.add(2, 3)
    return total * 10
end
function M.loop()
    local sum = 0
    for i = 1, 6 do
        sum = sum + i
    end
    return sum
end
function M.fail(value)
    local doubled = value * 2
    error("broken " .. doubled)
end
function M.inspect()
    local probe = setmetatable({}, {__name = "Probe", __index = function() touched = true end, __pairs = function() touched = true end, __len = function() touched = true end, __tostring = function() touched = true return "" end}) local camera = require("haylen.graphics2d").newCamera()
    return probe, camera
end
function M.request()
    local reason = "asked"
    require("haylen.debug").breakpoint()
    return reason
end
return M
)";

    static constexpr const char* kTask = R"(local async = require("async")
return function()
    async.spawn(function()
        local before = 1
        async.sleep(1):await()
        local after = before + 1
        finished = after
    end)
end
)";

    // Runs the source through the message handler of the engine and returns the message of its error, or an empty text.
    static std::string run(lua_State* L, const std::string& source) {
        try {
            lua::Runtime::runChunk(L, source, "=test");
        } catch (const lua::Error& error) {
            return error.what();
        }
        return {};
    }

    static core::Json breakpoints(std::vector<core::Json> list) {
        return {{"source", {{"path", "source/mod.lua"}}}, {"breakpoints", std::move(list)}};
    }

    static constexpr const char* kGenerator = R"(return function()
    local value = 1
    coroutine.yield(value)
    value = value + 1
end
)";

    test::EngineFixture fixture{{{"source/mod.lua", kModule}, {"source/task.lua", kTask}, {"source/generator.lua", kGenerator}}};
    test::DebugClient client{fixture.host()};
};

class DebuggerStepTest : public DebuggerTest {};

class DebuggerErrorTest : public DebuggerTest {};

class DebuggerWebTest : public DebuggerTest {};

TEST_F(DebuggerTest, HaltsAtABreakpointWithTheStackScopesAndValues) {
    client.connect(fixture.lua());
    const core::Json capabilities = client.getResponse(1).at("body");
    EXPECT_TRUE(capabilities.at("supportsConditionalBreakpoints").get<bool>());
    EXPECT_EQ(capabilities.at("exceptionBreakpointFilters").at(0).at("filter"), "uncaught");
    EXPECT_EQ(client.getEvents("initialized").size(), 1U);

    const int set = client.request("setBreakpoints", breakpoints({{{"line", 5}}}));
    client.getDebugger().update(fixture.lua());
    EXPECT_EQ(client.getResponse(set).at("body").at("breakpoints").at(0).at("verified"), true);

    int frames = 0;
    int scopes = 0;
    int locals = 0;
    int info = 0;
    int evaluation = 0;
    client.then([&](test::DebugClient& debug) { frames = debug.request("stackTrace", {{"threadId", 1}}); });
    client.then([&](test::DebugClient& debug) { scopes = debug.request("scopes", {{"frameId", 1}}); });
    // clang-format off
    client.then([&](test::DebugClient& debug) {
        locals = debug.request("variables", {{"variablesReference", debug.getResponse(scopes).at("body").at("scopes").at(0).at("variablesReference")}});
        evaluation = debug.request("evaluate", {{"expression", "sum * 2, a"}, {"frameId", 1}});
    });
    client.then([&](test::DebugClient& debug) {
        for (const core::Json& variable : debug.getResponse(locals).at("body").at("variables")) {
            if (variable.at("name") == "info") {
                info = debug.request("variables", {{"variablesReference", variable.at("variablesReference")}});
            }
        }
    });
    // clang-format on
    client.then([](test::DebugClient& debug) { debug.request("continue", {{"threadId", 1}}); });
    EXPECT_EQ(run(fixture.lua(), "result = require('mod').run()"), "");
    EXPECT_EQ(fixture.lua("return result"), "50");
    EXPECT_FALSE(client.hasSteps());

    const std::vector<core::Json> stops = client.getEvents("stopped");
    ASSERT_EQ(stops.size(), 1U);
    EXPECT_EQ(stops.front().at("reason"), "breakpoint");
    EXPECT_EQ(stops.front().at("hitBreakpointIds"), core::Json::array({client.getResponse(set).at("body").at("breakpoints").at(0).at("id")}));

    const core::Json stack = client.getResponse(frames).at("body").at("stackFrames");
    EXPECT_EQ(stack.at(0).at("name"), "add");
    EXPECT_EQ(stack.at(0).at("line"), 5);
    EXPECT_EQ(stack.at(0).at("source").at("path"), "source/mod.lua");
    EXPECT_EQ(stack.at(1).at("name"), "run");
    EXPECT_EQ(stack.at(1).at("line"), 8);

    std::map<std::string, std::string> values;
    for (const core::Json& variable : client.getResponse(locals).at("body").at("variables")) {
        values[variable.at("name").get<std::string>()] = variable.at("value").get<std::string>();
    }
    EXPECT_EQ(values.at("a"), "2");
    EXPECT_EQ(values.at("b"), "3");
    EXPECT_EQ(values.at("sum"), "5");
    EXPECT_TRUE(values.at("info").starts_with("table: "));
    const core::Json fields = client.getResponse(info).at("body").at("variables");
    EXPECT_EQ(fields.at(0).at("name"), "name");
    EXPECT_EQ(fields.at(0).at("value"), "\"point\"");
    EXPECT_EQ(fields.at(1).at("name"), "values");
    EXPECT_GT(fields.at(1).at("variablesReference"), 0);
    EXPECT_EQ(client.getResponse(evaluation).at("body").at("result"), "10, 2");
}

// Only the frame thread reads the hooks, so a coroutine that a test resumes halts as well, and nothing of the app runs while the client looks.
TEST_F(DebuggerTest, ConditionsCountsAndLogPointsDecideWhetherALineHalts) {
    client.connect(fixture.lua());
    client.request("setBreakpoints", breakpoints({{{"line", 14}, {"condition", "i == 4"}}}));
    client.getDebugger().update(fixture.lua());
    int value = 0;
    client.then([&](test::DebugClient& debug) { value = debug.request("evaluate", {{"expression", "i"}, {"frameId", 1}}); });
    client.then([](test::DebugClient& debug) { debug.request("continue"); });
    EXPECT_EQ(run(fixture.lua(), "result = require('mod').loop()"), "");
    EXPECT_EQ(client.getEvents("stopped").size(), 1U);
    EXPECT_EQ(client.getResponse(value).at("body").at("result"), "4");

    client.request("setBreakpoints", breakpoints({{{"line", 14}, {"hitCondition", "%2"}}}));
    client.getDebugger().update(fixture.lua());
    for (int halt = 0; halt < 3; ++halt) {
        client.then([](test::DebugClient& debug) { debug.request("continue"); });
    }
    EXPECT_EQ(run(fixture.lua(), "result = require('mod').loop()"), "");
    EXPECT_EQ(client.getEvents("stopped").size(), 4U);

    client.request("setBreakpoints", breakpoints({{{"line", 14}, {"logMessage", "i is {i}, sum {sum}"}}}));
    client.getDebugger().update(fixture.lua());
    EXPECT_EQ(run(fixture.lua(), "result = require('mod').loop()"), "");
    EXPECT_EQ(client.getEvents("stopped").size(), 4U);
    const std::vector<core::Json> outputs = client.getEvents("output");
    ASSERT_EQ(outputs.size(), 6U);
    EXPECT_EQ(outputs.front().at("output"), "i is 1, sum 0\n");
    EXPECT_EQ(outputs.back().at("output"), "i is 6, sum 15\n");
    EXPECT_EQ(fixture.lua("return result"), "21");

    // A hit count that is no count never takes effect, and a condition that fails halts with its error.
    const int invalid = client.request("setBreakpoints", breakpoints({{{"line", 14}, {"hitCondition", "often"}}, {{"line", 16}, {"condition", "nothing + 1"}}}));
    client.getDebugger().update(fixture.lua());
    const core::Json results = client.getResponse(invalid).at("body").at("breakpoints");
    EXPECT_EQ(results.at(0).at("verified"), false);
    EXPECT_EQ(results.at(1).at("verified"), true);
    client.then([](test::DebugClient& debug) { debug.request("continue"); });
    EXPECT_EQ(run(fixture.lua(), "require('mod').loop()"), "");
    EXPECT_EQ(client.getEvents("stopped").size(), 5U);
    EXPECT_NE(client.getEvents("output").back().at("output").get<std::string>().find("The condition \"nothing + 1\" of the breakpoint at line 16 failed"), std::string::npos);
}

TEST_F(DebuggerStepTest, StepsInOutAndOverTheCallsOfAFunction) {
    client.connect(fixture.lua());
    client.request("setBreakpoints", breakpoints({{{"line", 8}}}));
    client.getDebugger().update(fixture.lua());
    std::vector<int> traces;
    // clang-format off
    const auto trace = [&traces](const char* command) {
        return [&traces, command](test::DebugClient& debug) {
            traces.push_back(debug.request("stackTrace", {{"threadId", 1}}));
            debug.request(command, {{"threadId", 1}});
        };
    };
    // clang-format on
    client.then(trace("stepIn"));
    client.then(trace("next"));
    client.then(trace("stepOut"));
    client.then(trace("continue"));
    EXPECT_EQ(run(fixture.lua(), "result = require('mod').run()"), "");
    EXPECT_EQ(fixture.lua("return result"), "50");

    std::vector<int> lines;
    for (const int request : traces) {
        lines.push_back(client.getResponse(request).at("body").at("stackFrames").at(0).at("line").get<int>());
    }
    EXPECT_EQ(lines, (std::vector<int>{8, 3, 4, 9}));
    const std::vector<core::Json> stops = client.getEvents("stopped");
    ASSERT_EQ(stops.size(), 4U);
    EXPECT_EQ(stops.at(1).at("reason"), "step");
}

// A step over the await of a task of the async library waits for the task to go on in a later frame, while the main thread and other code run without halting.
TEST_F(DebuggerStepTest, StepsOverTheAwaitOfATaskIntoTheFrameWhereItGoesOn) {
    client.connect(fixture.lua());
    client.request("setBreakpoints", {{"source", {{"path", "source/task.lua"}}}, {"breakpoints", {{{"line", 4}}}}});
    client.getDebugger().update(fixture.lua());
    std::vector<int> traces;
    for (const char* command : {"next", "next", "next", "continue"}) {
        // clang-format off
        client.then([&traces, command](test::DebugClient& debug) {
            traces.push_back(debug.request("stackTrace"));
            debug.request(command);
        });
        // clang-format on
    }
    EXPECT_EQ(run(fixture.lua(), "require('task')()"), "");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return finished") == "2"; }));
    EXPECT_FALSE(client.hasSteps());

    std::vector<int> lines;
    for (const int request : traces) {
        const core::Json frame = client.getResponse(request).at("body").at("stackFrames").at(0);
        EXPECT_EQ(frame.at("source").at("path"), "source/task.lua");
        lines.push_back(frame.at("line").get<int>());
    }
    EXPECT_EQ(lines, (std::vector<int>{4, 5, 6, 7}));
}

// A coroutine that waited since before the debugger watched anything takes the hooks when the debugger starts to watch, so a pause halts at its next line when native code resumes it.
TEST_F(DebuggerStepTest, PausesInACoroutineThatWaitedBeforeTheDebuggerWatched) {
    fixture.runLua(R"(
        resumed = 0
        waiting = coroutine.create(function()
            coroutine.yield()
            resumed = resumed + 1
        end)
        coroutine.resume(waiting)
    )");
    client.connect(fixture.lua());
    client.request("pause");
    client.getDebugger().update(fixture.lua());
    int frames = 0;
    // clang-format off
    client.then([&](test::DebugClient& debug) {
        frames = debug.request("stackTrace");
        debug.request("continue");
    });
    // clang-format on

    lua_State* L = fixture.lua();
    lua_getglobal(L, "waiting");
    lua_State* waiting = lua_tothread(L, -1);
    int results = 0;
    EXPECT_EQ(lua_resume(waiting, L, 0, &results), LUA_OK);
    lua_pop(L, 1);
    EXPECT_EQ(client.getResponse(frames).at("body").at("stackFrames").at(0).at("line"), 5);
    EXPECT_EQ(fixture.lua("return resumed"), "1");
}

// A step into a coroutine that yields waits for it to go on, and a coroutine that the app drops meanwhile stays whole while the lines of the app ask whether the step is done.
TEST_F(DebuggerStepTest, StepsInAtTheYieldOfACoroutineThatTheAppThenDrops) {
    client.connect(fixture.lua());
    client.request("setBreakpoints", {{"source", {{"path", "source/generator.lua"}}}, {"breakpoints", {{{"line", 3}}}}});
    client.getDebugger().update(fixture.lua());
    client.then([](test::DebugClient& debug) { debug.request("stepIn"); });

    EXPECT_EQ(run(fixture.lua(), "generator = coroutine.wrap(require('generator')) generator()"), "");
    EXPECT_EQ(run(fixture.lua(), "generator = nil collectgarbage() collectgarbage() result = 0 for index = 1, 3 do result = result + index end"), "");
    EXPECT_EQ(fixture.lua("return result"), "6");
    EXPECT_EQ(client.getEvents("stopped").size(), 1U);
}

TEST_F(DebuggerErrorTest, HaltsWhereAnUncaughtErrorRisesBeforeItsStackUnwinds) {
    client.connect(fixture.lua());
    client.request("setExceptionBreakpoints", {{"filters", {"uncaught"}}});
    client.getDebugger().update(fixture.lua());

    // An error that a protected call of the app catches never halts.
    EXPECT_EQ(run(fixture.lua(), "caught = pcall(require('mod').fail, 1)"), "");
    EXPECT_TRUE(client.getEvents("stopped").empty());

    int frames = 0;
    int scopes = 0;
    int locals = 0;
    client.then([&](test::DebugClient& debug) { frames = debug.request("stackTrace"); });
    client.then([&](test::DebugClient& debug) { scopes = debug.request("scopes", {{"frameId", 2}}); });
    client.then([&](test::DebugClient& debug) { locals = debug.request("variables", {{"variablesReference", debug.getResponse(scopes).at("body").at("scopes").at(0).at("variablesReference")}}); });
    client.then([](test::DebugClient& debug) { debug.request("next"); });
    EXPECT_NE(run(fixture.lua(), "require('mod').fail(21)").find("broken 42"), std::string::npos);

    const std::vector<core::Json> stops = client.getEvents("stopped");
    ASSERT_EQ(stops.size(), 1U);
    EXPECT_EQ(stops.front().at("reason"), "exception");
    EXPECT_NE(stops.front().at("text").get<std::string>().find("broken 42"), std::string::npos);
    const core::Json stack = client.getResponse(frames).at("body").at("stackFrames");
    EXPECT_EQ(stack.at(0).at("name"), "error");
    EXPECT_EQ(stack.at(1).at("name"), "fail");
    EXPECT_EQ(stack.at(1).at("line"), 20);
    const core::Json variables = client.getResponse(locals).at("body").at("variables");
    EXPECT_EQ(variables.at(1).at("name"), "doubled");
    EXPECT_EQ(variables.at(1).at("value"), "42");
}

TEST_F(DebuggerErrorTest, HaltsOnTheErrorOfATaskOfTheAsyncLibrary) {
    client.connect(fixture.lua());
    client.request("setExceptionBreakpoints", {{"filters", {"uncaught"}}});
    client.getDebugger().update(fixture.lua());
    int frames = 0;
    client.then([&](test::DebugClient& debug) { frames = debug.request("stackTrace"); });
    client.then([](test::DebugClient& debug) { debug.request("continue"); });
    fixture.runLua("require('async').spawn(function() local mod = require('mod') mod.fail(5) end)");
    fixture.frames(2);
    ASSERT_EQ(client.getEvents("stopped").size(), 1U);
    EXPECT_EQ(client.getResponse(frames).at("body").at("stackFrames").at(1).at("line"), 20);
    EXPECT_NE(fixture.engine().getError(), nullptr);
}

// Looking at a value reads it raw, so the metamethods of the app never run, and an evaluation that never ends stops at its time bound.
TEST_F(DebuggerTest, InspectsValuesWithoutRunningMetamethodsAndBoundsEvaluations) {
    client.connect(fixture.lua());
    client.request("setBreakpoints", breakpoints({{{"line", 24}}}));
    client.getDebugger().update(fixture.lua());
    int locals = 0;
    int probe = 0;
    int metatable = 0;
    int camera = 0;
    int endless = 0;
    client.then([](test::DebugClient& debug) { debug.request("scopes", {{"frameId", 1}}); });
    client.then([&](test::DebugClient& debug) { locals = debug.request("variables", {{"variablesReference", 1}}); });
    client.then([&](test::DebugClient& debug) { probe = debug.request("variables", {{"variablesReference", debug.getResponse(locals).at("body").at("variables").at(0).at("variablesReference")}}); });
    client.then([&](test::DebugClient& debug) { metatable = debug.request("variables", {{"variablesReference", debug.getResponse(probe).at("body").at("variables").at(0).at("variablesReference")}}); });
    client.then([&](test::DebugClient& debug) { camera = debug.request("variables", {{"variablesReference", debug.getResponse(locals).at("body").at("variables").at(1).at("variablesReference")}}); });
    client.then([&](test::DebugClient& debug) { endless = debug.request("evaluate", {{"expression", "(function() while true do end end)()"}, {"frameId", 1}}); });
    client.then([](test::DebugClient& debug) { debug.request("continue"); });
    const auto started = std::chrono::steady_clock::now();
    EXPECT_EQ(run(fixture.lua(), "require('mod').inspect()"), "");
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(5));

    const core::Json shown = client.getResponse(locals).at("body").at("variables").at(0);
    EXPECT_EQ(shown.at("name"), "probe");
    EXPECT_TRUE(shown.at("value").get<std::string>().starts_with("Probe: "));
    EXPECT_EQ(client.getResponse(probe).at("body").at("variables").at(0).at("name"), "(metatable)");
    EXPECT_EQ(client.getResponse(metatable).at("body").at("variables").size(), 5U);
    EXPECT_EQ(fixture.lua("return touched"), "nil");

    // A bound object lists the properties of its type, which the getters of the engine read.
    std::map<std::string, std::string> properties;
    for (const core::Json& variable : client.getResponse(camera).at("body").at("variables")) {
        properties[variable.at("name").get<std::string>()] = variable.at("value").get<std::string>();
    }
    EXPECT_TRUE(properties.at("zoom").starts_with("haylen.Vec2: "));
    EXPECT_EQ(properties.at("rotation"), "0.0");
    EXPECT_TRUE(properties.contains("(metatable)"));
    EXPECT_FALSE(client.getResponse(endless).at("success").get<bool>());
    EXPECT_NE(client.getResponse(endless).at("message").get<std::string>().find("ran longer than 1000 ms"), std::string::npos);
}

TEST_F(DebuggerTest, HaltsWhereTheAppCallsBreakpointOnlyWhileAClientIsAttached) {
    EXPECT_EQ(run(fixture.lua(), "require('mod').request()"), "");
    client.connect(fixture.lua());
    int frames = 0;
    // clang-format off
    client.then([&](test::DebugClient& debug) {
        frames = debug.request("stackTrace");
        debug.request("continue");
    });
    // clang-format on
    EXPECT_EQ(run(fixture.lua(), "require('mod').request()"), "");
    ASSERT_EQ(client.getEvents("stopped").size(), 1U);
    EXPECT_EQ(client.getEvents("stopped").front().at("reason"), "pause");
    EXPECT_EQ(client.getResponse(frames).at("body").at("stackFrames").at(0).at("line"), 28);
}

// A pause halts the next line of the app, and the platform may ask the frame to finish, which lets the app go on without halting until the next frame.
TEST_F(DebuggerTest, PausesAtTheNextLineAndLetsTheFrameFinishWhenThePlatformAsks) {
    client.connect(fixture.lua());
    client.request("pause");
    client.getDebugger().update(fixture.lua());
    client.then([](test::DebugClient& debug) { debug.request("continue"); });
    EXPECT_EQ(run(fixture.lua(), "require('mod').loop()"), "");
    ASSERT_EQ(client.getEvents("stopped").size(), 1U);
    EXPECT_EQ(client.getEvents("stopped").front().at("reason"), "pause");

    client.request("setBreakpoints", breakpoints({{{"line", 14}}}));
    client.getDebugger().update(fixture.lua());
    fixture.host().setFinishingHalts(true);
    EXPECT_EQ(run(fixture.lua(), "result = require('mod').loop()"), "");
    EXPECT_EQ(fixture.lua("return result"), "21");
    EXPECT_EQ(client.getEvents("stopped").size(), 2U);
    EXPECT_EQ(client.getEvents("continued").size(), 1U);

    // The next frame halts again.
    fixture.host().setFinishingHalts(false);
    client.getDebugger().update(fixture.lua());
    client.then([](test::DebugClient& debug) { debug.request("disconnect"); });
    EXPECT_EQ(run(fixture.lua(), "require('mod').loop()"), "");
    EXPECT_EQ(client.getEvents("stopped").size(), 3U);
    EXPECT_FALSE(client.getDebugger().isAttached());
}

// The breakpoints belong to the debugger, which outlives the Lua state of an app that restarts.
TEST_F(DebuggerTest, KeepsTheBreakpointsWhenTheAppRestarts) {
    client.connect(fixture.lua());
    client.request("setBreakpoints", breakpoints({{{"line", 3}}}));
    client.getDebugger().update(fixture.lua());
    client.getDebugger().detach();
    fixture.restart();
    client.getDebugger().attach(fixture.lua());
    client.then([](test::DebugClient& debug) { debug.request("continue"); });
    EXPECT_EQ(run(fixture.lua(), "require('mod').run()"), "");
    EXPECT_EQ(client.getEvents("stopped").size(), 1U);
}

// A web page cannot halt, so it keeps the log points and says why the other breakpoints never take effect.
TEST_F(DebuggerWebTest, KeepsOnlyTheLogPointsOnAPageThatCannotHalt) {
    fixture.host().setHaltable(false);
    client.connect(fixture.lua());
    EXPECT_TRUE(client.getResponse(1).at("body").at("exceptionBreakpointFilters").empty());
    const int set = client.request("setBreakpoints", breakpoints({{{"line", 14}}, {{"line", 16}, {"logMessage", "sum {sum}"}}}));
    const int pause = client.request("pause");
    client.getDebugger().update(fixture.lua());
    const core::Json results = client.getResponse(set).at("body").at("breakpoints");
    EXPECT_EQ(results.at(0).at("verified"), false);
    EXPECT_NE(results.at(0).at("message").get<std::string>().find("cannot halt"), std::string::npos);
    EXPECT_EQ(results.at(1).at("verified"), true);
    EXPECT_FALSE(client.getResponse(pause).at("success").get<bool>());

    EXPECT_EQ(run(fixture.lua(), "require('mod').loop()"), "");
    EXPECT_TRUE(client.getEvents("stopped").empty());
    EXPECT_EQ(client.getEvents("output").back().at("output"), "sum 21\n");
}

} // namespace haylen::debug
