#include <gtest/gtest.h>

#include <lua.hpp>

#include <chrono>
#include <memory>
#include <thread>

#include "debug/Evaluation.hpp"

namespace haylen::debug {

// Runs evaluations on a Lua state of its own, with the globals as their environment.
class EvaluationTest : public ::testing::Test {
  protected:
    std::unique_ptr<lua_State, decltype(&lua_close)> state{luaL_newstate(), &lua_close};

    void SetUp() override {
        luaL_openlibs(state.get());
    }

    Evaluation::Result evaluate(const char* source, std::chrono::milliseconds limit) {
        lua_State* L = state.get();
        lua_pushglobaltable(L);
        const Evaluation::Result result = Evaluation::run(L, -1, source, limit);
        lua_settop(L, 0);
        return result;
    }
};

// A task that an evaluation spawns, such as with the async library, waits and goes on long after the evaluation ended, as code of the app.
TEST_F(EvaluationTest, LeavesTheTimeLimitToTheEvaluationAndNotToTheCoroutinesItCreated) {
    const Evaluation::Result started = evaluate("task = coroutine.create(function() coroutine.yield() local sum = 0 for i = 1, 100000 do sum = sum + i end return sum end) coroutine.resume(task)", std::chrono::milliseconds(1));
    ASSERT_TRUE(started.ok) << started.error;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    lua_State* L = state.get();
    lua_getglobal(L, "task");
    lua_State* task = lua_tothread(L, -1);
    int results = 0;
    const int status = lua_resume(task, L, 0, &results);

    ASSERT_EQ(status, LUA_OK) << lua_tostring(task, -1);
    EXPECT_EQ(lua_tointeger(task, -1), 5000050000);
}

// The results move onto a stack that grows for them, however many the evaluation returns.
TEST_F(EvaluationTest, ReturnsMoreValuesThanTheStackOfTheCallerHeld) {
    lua_State* L = state.get();
    lua_pushglobaltable(L);
    const Evaluation::Result result = Evaluation::run(L, -1, "table.unpack({}, 1, 5000)", std::chrono::milliseconds(1000));

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.count, 5000);
    EXPECT_EQ(lua_gettop(L), 5001);
    lua_settop(L, 0);
}

} // namespace haylen::debug
