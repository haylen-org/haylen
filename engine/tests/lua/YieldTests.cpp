#include <gtest/gtest.h>
#include <lua.hpp>

#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Error.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::lua {

// A yield jumps back to its resume without running the destructors of the native frames it leaves, so each native function of the engine that calls Lua through a continuation is yielded through many times here, with errors raised after the yields, which the address sanitizer checks.
class YieldTest : public ::testing::Test {
  protected:
    bool finishes(const std::string& condition) {
        return fixture.frameUntil([&] { return fixture.lua("return tostring(" + condition + ")") == "true"; });
    }

    test::EngineFixture fixture;
};

TEST_F(YieldTest, PassesThroughTasksAndThePromisesTheyReturn) {
    // clang-format off
    fixture.runLua(R"(
        async = require('async')
        scene = require('haylen.scene')
        owner = {}
        finished, caught = 0, 0
        for round = 1, 200 do
            scene.spawn(owner, function()
                local ok = pcall(function()
                    async.sleep(0):await()
                    error('A failure after a yield.')
                end)
                caught = caught + (ok and 0 or 1)
                return async.promise(function()
                    async.yield():await()
                    finished = finished + 1
                end)
            end)
        end
    )");
    // clang-format on
    ASSERT_TRUE(finishes("finished == 200"));
    EXPECT_EQ(fixture.lua("return caught"), "200");
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    fixture.runLua("scene.spawn(owner, function() async.yield():await() error('The task failed after a yield.', 0) end)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; }));
    EXPECT_EQ(fixture.engine().getError()->getMessage(), "The task failed after a yield.");
}

TEST_F(YieldTest, PassesThroughDofile) {
    test::TemporaryDirectory folder;
    folder.write("wait.lua", "local async = require('async')\nasync.sleep(0):await()\nreturn pcall(function() async.yield():await() error('A failure after a yield.') end)\n");
    lua_State* L = fixture.lua();
    lua_pushstring(L, (folder.getPath() / "wait.lua").generic_string().c_str());
    lua_setglobal(L, "path");

    // clang-format off
    fixture.runLua(R"(
        async = require('async')
        finished = 0
        for round = 1, 200 do
            async.spawn(function()
                if not dofile(path) then
                    finished = finished + 1
                end
            end)
        end
    )");
    // clang-format on
    ASSERT_TRUE(finishes("finished == 200"));
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(YieldTest, PassesThroughTheAwaitOfPlatformCalls) {
    // clang-format off
    fixture.runLua(R"(
        async = require('async')
        platform = require('haylen.platform')
        platform.registerHandler('test.two', function() return 2 end)
        total, timeouts = 0, 0
        for round = 1, 200 do
            async.spawn(function()
                local value = platform.call('test.two'):await()
                total = total + value
                local _, failure = platform.call('test.silent', nil, {timeout = 0.01}):await()
                timeouts = timeouts + (failure.code == 'timeout' and 1 or 0)
            end)
        end
    )");
    // clang-format on
    ASSERT_TRUE(finishes("timeouts == 200"));
    EXPECT_EQ(fixture.lua("return total"), "400");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(YieldTest, PassesThroughThePageLoaderOfACollection) {
    // clang-format off
    fixture.runLua(R"(
        async = require('async')
        ui = require('haylen.ui')
        loads = 0
        gui = ui.mount(ui.column{ui.collection{id = 'pages', height = 600, types = {
            entry = {template = ui.label{bind = {text = 'title'}, height = 60}},
        }}})
        pages = gui:collection('pages')
        pages:setPages({count = 1000, pageSize = 5, load = function(first, count)
            return async.promise(function()
                async.yield():await()
                pcall(function() async.sleep(0):await() error('A failure after a yield.') end)
                local page = {}
                for index = 1, count do
                    page[index] = {id = 'e' .. (first + index - 1), type = 'entry', title = 'Entry ' .. (first + index - 1)}
                end
                loads = loads + 1
                return page
            end)
        end})
    )");
    // clang-format on
    ASSERT_TRUE(finishes("pages:cellOf(1) ~= nil and pages:cellOf(1).type == 'entry'"));
    EXPECT_NE(fixture.lua("return loads"), "0");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(YieldTest, PassesThroughJobCheckpoints) {
    // clang-format off
    fixture.runLua(R"(
        async = require('async')
        jobs = require('haylen.jobs')
        jobs.setBudget(0.02)
        async.spawn(function()
            local work = {}
            for round = 1, 10 do
                work[round] = jobs.spawn(function()
                    local sum = 0
                    for index = 1, 1000 do
                        sum = sum + index
                        jobs.checkpoint()
                    end
                    return sum
                end)
            end
            local sums = async.all(work):await()
            total = 0
            for _, sum in ipairs(sums) do
                total = total + sum
            end
        end)
    )");
    // clang-format on
    ASSERT_TRUE(finishes("total ~= nil"));
    EXPECT_EQ(fixture.lua("return total"), "5005000");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen::lua
