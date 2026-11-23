#pragma once

#include <gtest/gtest.h>

#include <lua.hpp>

#include <string>
#include <utility>
#include <vector>

#include "haylen/core/AppError.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/lua/Runtime.hpp"
#include "support/DebugClient.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::debug {

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

    static constexpr const char* kGenerator = R"(return function()
    local value = 1
    coroutine.yield(value)
    value = value + 1
end
)";

    // Runs the source through the message handler of the engine and returns the message of its error, or an empty text.
    static std::string run(lua_State* L, const std::string& source) {
        try {
            lua::Runtime::runChunk(L, source, "=test");
        } catch (const haylen::core::AppError& error) {
            return error.what();
        }

        return {};
    }

    static haylen::core::Json breakpoints(std::vector<haylen::core::Json> list) {
        return {{"source", {{"path", "source/mod.lua"}}}, {"breakpoints", std::move(list)}};
    }

    test::LuaFixture fixture{{{"source/mod.lua", kModule}, {"source/task.lua", kTask}, {"source/generator.lua", kGenerator}}};
    test::DebugClient client{fixture.host()};
};

} // namespace haylen::lua::debug
