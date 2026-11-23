#include <gtest/gtest.h>

#include <lua.hpp>

#include <vector>

#include "debug/DebuggerTest.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::lua::debug {

class DebuggerStepTest : public DebuggerTest {};

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
    const std::vector<haylen::core::Json> stops = client.getEvents("stopped");
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
        const haylen::core::Json frame = client.getResponse(request).at("body").at("stackFrames").at(0);
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

} // namespace haylen::lua::debug
