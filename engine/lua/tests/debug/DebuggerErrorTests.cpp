#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "debug/DebuggerTest.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::debug {

class DebuggerErrorTest : public DebuggerTest {};

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

} // namespace haylen::debug
