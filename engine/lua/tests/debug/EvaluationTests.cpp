#include <gtest/gtest.h>

#include <lua.hpp>

#include <chrono>
#include <memory>
#include <thread>

#include "debug/Evaluation.hpp"

namespace haylen::lua::debug {

// Runs evaluations on a Lua state of its own, with the globals as their environment.
class EvaluationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        luaL_openlibs(state.get());
    }

    lua::debug::Evaluation::Result evaluate(const char* source, std::chrono::milliseconds limit) {
        lua_State* L = state.get();
        lua_pushglobaltable(L);
        const lua::debug::Evaluation::Result result = lua::debug::Evaluation::run(L, -1, source, limit);
        lua_settop(L, 0);
        return result;
    }

    std::unique_ptr<lua_State, decltype(&lua_close)> state{luaL_newstate(), &lua_close};
};

// A task that an evaluation spawns, such as with the async library, waits and goes on long after the evaluation ended, as code of the app.
TEST_F(EvaluationTest, LeavesTheTimeLimitToTheEvaluationAndNotToTheCoroutinesItCreated) {
    const lua::debug::Evaluation::Result started = evaluate("task = coroutine.create(function() coroutine.yield() local sum = 0 for i = 1, 100000 do sum = sum + i end return sum end) coroutine.resume(task)", std::chrono::milliseconds(1));
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

// An evaluation nests in another when the code of the outer one reaches a breakpoint whose condition evaluates, and the outer one keeps its time limit once the inner one ended.
TEST_F(EvaluationTest, KeepsTheTimeLimitOfAnEvaluationAfterAnotherNestedInIt) {
    lua_State* L = state.get();
    // clang-format off
    lua_pushcfunction(L, [](lua_State* inner) {
        lua_pushglobaltable(inner);
        const lua::debug::Evaluation::Result condition = lua::debug::Evaluation::run(inner, -1, "true", lua::debug::Evaluation::kTimeLimit);
        lua_pushboolean(inner, condition.ok ? 1 : 0);
        return 1;
    });
    // clang-format on
    lua_setglobal(L, "condition");

    const lua::debug::Evaluation::Result result = evaluate("assert(condition()) for _ = 1, 300000000 do end", std::chrono::milliseconds(100));
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, "The evaluation ran longer than 100 ms, so it stopped.");
}

// The results move onto a stack that grows for them, however many the evaluation returns.
TEST_F(EvaluationTest, ReturnsMoreValuesThanTheStackOfTheCallerHeld) {
    lua_State* L = state.get();
    lua_pushglobaltable(L);
    const lua::debug::Evaluation::Result result = lua::debug::Evaluation::run(L, -1, "table.unpack({}, 1, 5000)", std::chrono::milliseconds(1000));

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.count, 5000);
    EXPECT_EQ(lua_gettop(L), 5001);
    lua_settop(L, 0);
}

} // namespace haylen::lua::debug
