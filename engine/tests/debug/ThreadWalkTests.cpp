#include <gtest/gtest.h>
#include <lua.hpp>

#include <memory>
#include <stdexcept>

#include "debug/ThreadWalk.hpp"

namespace haylen::debug {

class ThreadWalkTest : public ::testing::Test {
  protected:
    ThreadWalkTest() {
        luaL_openlibs(state.get());
        EXPECT_EQ(luaL_dostring(state.get(), "worker = coroutine.create(function() coroutine.yield() end) coroutine.resume(worker)"), LUA_OK);
    }

    // Walks from the coroutine that calls it and returns whether the walk visited the coroutine of its argument.
    static int findFrom(lua_State* L) {
        lua_State* wanted = lua_tothread(L, 1);
        bool found = false;
        lua::debug::ThreadWalk::run(L, [wanted, &found](lua_State* thread) { found = found || thread == wanted; });
        lua_pushboolean(L, found ? 1 : 0);
        return 1;
    }

    std::unique_ptr<lua_State, decltype(&lua_close)> state{luaL_newstate(), &lua_close};
};

// A coroutine that only the stack of the walking coroutine holds is still a coroutine of the heap.
TEST_F(ThreadWalkTest, VisitsTheCoroutinesThatTheWalkingCoroutineHolds) {
    lua_register(state.get(), "findFrom", &findFrom);
    // clang-format off
    ASSERT_EQ(luaL_dostring(state.get(), R"lua(
        local walker = coroutine.wrap(function()
            local inner = coroutine.create(function() coroutine.yield() end)
            coroutine.resume(inner)
            return findFrom(inner)
        end)
        found = walker()
    )lua"), LUA_OK) << lua_tostring(state.get(), -1);
    // clang-format on
    lua_getglobal(state.get(), "found");
    EXPECT_TRUE(lua_toboolean(state.get(), -1));
}

// The walk holds the collector, and a failure on the way gives it back as a walk that ends does.
TEST_F(ThreadWalkTest, RestartsTheCollectorWhenTheWalkFails) {
    EXPECT_THROW(lua::debug::ThreadWalk::run(state.get(), [](lua_State*) { throw std::runtime_error("The visit failed."); }), std::runtime_error);

    EXPECT_NE(lua_gc(state.get(), LUA_GCISRUNNING), 0);
}

} // namespace haylen::debug
