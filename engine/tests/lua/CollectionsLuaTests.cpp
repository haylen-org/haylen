#include <gtest/gtest.h>

#include <string>

#include "support/EngineFixture.hpp"

namespace haylen {

TEST(CollectionsLuaTest, PoolsRecycleObjects) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        collections = require('haylen.collections')
        created = 0
        released = 0
        pool = collections.newPool({
            create = function()
                created = created + 1
                return {id = created}
            end,
            reset = function(bullet, x, y)
                bullet.x = x
                bullet.y = y
            end,
            release = function(bullet)
                released = released + 1
                bullet.x = nil
            end,
            capacity = 3,
            prewarm = 2,
        })
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return created .. ' ' .. pool.idle .. ' ' .. pool.active .. ' ' .. pool.capacity"), "2 2 0 3");

    fixture.runLua("first = pool:acquire(10, 20) second = pool:acquire(1, 2) third = pool:acquire(3, 4)");
    EXPECT_EQ(fixture.lua("return first.x .. ' ' .. first.y .. ' ' .. created .. ' ' .. pool.active .. ' ' .. pool.idle"), "10 20 3 3 0");
    EXPECT_EQ(fixture.lua("return tostring(pool:acquire(0, 0))"), "nil");

    // Released objects come back on the next acquire instead of new ones.
    EXPECT_EQ(fixture.lua("return tostring(pool:release(second)) .. ' ' .. tostring(pool:release(second)) .. ' ' .. released"), "true false 1");
    EXPECT_EQ(fixture.lua("local again = pool:acquire(5, 6) return tostring(again == second) .. ' ' .. again.x .. ' ' .. created"), "true 5 3");

    // clang-format off
    fixture.runLua(R"(
        visited = 0
        pool:each(function(bullet)
            visited = visited + 1
            if bullet == first then pool:release(bullet) end
        end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return visited .. ' ' .. pool.active"), "3 2");
    EXPECT_EQ(fixture.lua("pool:releaseAll() return pool.active .. ' ' .. pool.idle .. ' ' .. released"), "0 3 4");
    EXPECT_EQ(fixture.lua("local open = collections.newPool({create = function() return {} end}) open:prewarm(5) return open.idle .. ' ' .. open.capacity"), "5 0");

    EXPECT_NE(fixture.lua("collections.newPool({})").find("create must be a function"), std::string::npos);
    EXPECT_NE(fixture.lua("collections.newPool({create = function() return {} end, size = 3})").find("Unknown option 'size'"), std::string::npos);
    EXPECT_NE(fixture.lua("collections.newPool({create = function() end}):acquire()").find("returned nil"), std::string::npos);
}

TEST(CollectionsLuaTest, RingBuffersKeepTheNewestValues) {
    test::EngineFixture fixture;
    fixture.runLua("collections = require('haylen.collections') history = collections.newRingBuffer(3)");
    EXPECT_EQ(fixture.lua("return tostring(history:push('a')) .. ' ' .. tostring(history:push({name = 'b'})) .. ' ' .. tostring(history:push(3))"), "false false false");
    EXPECT_EQ(fixture.lua("return tostring(history.full) .. ' ' .. tostring(history:push('d')) .. ' ' .. history.size .. ' ' .. history.capacity"), "true true 3 3");
    EXPECT_EQ(fixture.lua("return history:peek().name .. ' ' .. history:last() .. ' ' .. history:get(2) .. ' ' .. tostring(history:get(4))"), "b d 3 nil");
    EXPECT_EQ(fixture.lua("local values = history:values() return #values .. ' ' .. values[3]"), "3 d");
    EXPECT_EQ(fixture.lua("return history:pop().name .. ' ' .. history.size"), "b 2");
    EXPECT_EQ(fixture.lua("history:clear() return history.size .. ' ' .. tostring(history:pop()) .. ' ' .. tostring(history:peek()) .. ' ' .. tostring(history:last())"), "0 nil nil nil");
    EXPECT_NE(fixture.lua("collections.newRingBuffer(0)").find("capacity of at least one"), std::string::npos);
}

} // namespace haylen
