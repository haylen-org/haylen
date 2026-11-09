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

    std::unique_ptr<lua_State, decltype(&lua_close)> state{luaL_newstate(), &lua_close};
};

// The walk holds the collector, and a failure on the way gives it back as a walk that ends does.
TEST_F(ThreadWalkTest, RestartsTheCollectorWhenTheWalkFails) {
    EXPECT_THROW(ThreadWalk::run(state.get(), [](lua_State*) { throw std::runtime_error("The visit failed."); }), std::runtime_error);

    EXPECT_NE(lua_gc(state.get(), LUA_GCISRUNNING), 0);
}

} // namespace haylen::debug
