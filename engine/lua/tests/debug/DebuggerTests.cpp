#include <gtest/gtest.h>

#include <lua.hpp>

#include <algorithm>
#include <chrono>
#include <map>
#include <string>
#include <vector>

#include "debug/Debugger.hpp"
#include "debug/DebuggerTest.hpp"
#include "haylen/core/AppError.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/lua/Runtime.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/DebugClient.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::debug {

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
    ASSERT_FALSE(client.getEvents("output").empty());
    EXPECT_NE(client.getEvents("output").back().at("output").get<std::string>().find("The condition \"nothing + 1\" of the breakpoint at line 16 failed"), std::string::npos);
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

// A request with a field of another type than the protocol gives it fails with a message that names the field, and the debugger goes on serving the requests after it.
TEST_F(DebuggerTest, AnswersAMalformedRequestWithAnErrorThatNamesTheField) {
    client.connect(fixture.lua());
    const int line = client.request("setBreakpoints", breakpoints({{{"line", "3"}}}));
    const int arguments = client.request("setExceptionBreakpoints", {{"filters", "uncaught"}});
    const int wide = client.request("setBreakpoints", breakpoints({{{"line", 4294967301LL}}}));
    const int set = client.request("setBreakpoints", breakpoints({{{"line", 3}}}));
    client.getDebugger().update(fixture.lua());

    EXPECT_FALSE(client.getResponse(line).at("success").get<bool>());
    EXPECT_EQ(client.getResponse(line).at("message"), "The field \"line\" of the request must be an integer of 32 bits.");
    EXPECT_EQ(client.getResponse(arguments).at("message"), "The field \"filters\" of the request must be an array.");
    EXPECT_EQ(client.getResponse(wide).at("message"), "The field \"line\" of the request must be an integer of 32 bits.");
    EXPECT_EQ(client.getResponse(set).at("body").at("breakpoints").at(0).at("verified"), true);
}

TEST_F(DebuggerTest, RefusesABreakpointBeyondTheLastLineASourceReaches) {
    client.connect(fixture.lua());
    const int set = client.request("setBreakpoints", breakpoints({{{"line", 1048577}}, {{"line", 3}}}));
    client.getDebugger().update(fixture.lua());

    const core::Json results = client.getResponse(set).at("body").at("breakpoints");
    EXPECT_EQ(results.at(0).at("verified"), false);
    EXPECT_EQ(results.at(0).at("message"), "The line of a breakpoint is a whole number from 1 to 1048576.");
    EXPECT_EQ(results.at(1).at("verified"), true);
}

// Two breakpoints of one request on the same line leave the last one, and the result of the first one says so.
TEST_F(DebuggerTest, KeepsTheLastBreakpointOfALineAndSaysSoForTheFirst) {
    client.connect(fixture.lua());
    const int set = client.request("setBreakpoints", breakpoints({{{"line", 14}}, {{"line", 14}, {"logMessage", "sum {sum}"}}}));
    client.getDebugger().update(fixture.lua());

    const core::Json results = client.getResponse(set).at("body").at("breakpoints");
    EXPECT_EQ(results.at(0).at("verified"), false);
    EXPECT_EQ(results.at(0).at("message"), "A later breakpoint of the same request takes this line.");
    EXPECT_EQ(results.at(1).at("verified"), true);

    EXPECT_EQ(run(fixture.lua(), "require('mod').loop()"), "");
    EXPECT_TRUE(client.getEvents("stopped").empty());
    const std::vector<core::Json> outputs = client.getEvents("output");
    const auto logged = std::ranges::count_if(outputs, [](const core::Json& output) { return output.at("output").get<std::string>().starts_with("sum "); });
    EXPECT_EQ(logged, 6);
}

} // namespace haylen::debug
