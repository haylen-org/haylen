#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/ObjectPool.hpp"
#include "haylen/core/RingBuffer.hpp"

namespace haylen::core {

TEST(ObjectPoolTest, ReusesSlotsAndDetectsReleasedHandles) {
    ObjectPool<std::string> pool;
    pool.reserve(2);
    EXPECT_EQ(pool.getCapacity(), 2U);
    EXPECT_TRUE(pool.empty());

    const auto first = pool.acquire("bullet");
    const auto second = pool.acquire(3, 'x');
    std::string* address = pool.get(first);
    ASSERT_NE(address, nullptr);
    EXPECT_EQ(*address, "bullet");
    EXPECT_EQ(*pool.get(second), "xxx");
    EXPECT_EQ(pool.size(), 2U);

    // Growing the pool keeps live objects where they are.
    for (int index = 0; index < 100; ++index) {
        (void)pool.acquire("filler");
    }
    EXPECT_EQ(pool.get(first), address);

    EXPECT_TRUE(pool.release(first));
    EXPECT_FALSE(pool.release(first));
    EXPECT_EQ(pool.get(first), nullptr);

    // The freed slot takes the next object, and the old handle still misses it.
    const auto third = pool.acquire("rocket");
    EXPECT_EQ(third.index, first.index);
    EXPECT_NE(third, first);
    EXPECT_FALSE(pool.isAlive(first));
    EXPECT_EQ(*pool.get(third), "rocket");

    int live = 0;
    pool.forEach([&live](std::string&, ObjectPool<std::string>::Handle) { ++live; });
    EXPECT_EQ(live, 102);

    const std::size_t capacity = pool.getCapacity();
    pool.clear();
    EXPECT_TRUE(pool.empty());
    EXPECT_EQ(pool.getCapacity(), capacity);
    EXPECT_EQ(pool.get(second), nullptr);
}

TEST(ObjectPoolTest, DestroysObjectsOnRelease) {
    const auto tracker = std::make_shared<int>(0);
    ObjectPool<std::shared_ptr<int>> pool;
    const auto handle = pool.acquire(tracker);
    EXPECT_EQ(tracker.use_count(), 2);
    pool.release(handle);
    EXPECT_EQ(tracker.use_count(), 1);
}

TEST(RingBufferTest, DropsTheOldestElementWhenFull) {
    RingBuffer<int> buffer(3);
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.push(1));
    EXPECT_FALSE(buffer.push(2));
    EXPECT_FALSE(buffer.push(3));
    EXPECT_TRUE(buffer.full());
    EXPECT_TRUE(buffer.push(4));

    EXPECT_EQ(buffer.size(), 3U);
    EXPECT_EQ(buffer.front(), 2);
    EXPECT_EQ(buffer.back(), 4);
    EXPECT_EQ(buffer[1], 3);
    EXPECT_EQ(std::vector<int>(buffer.begin(), buffer.end()), (std::vector<int>{2, 3, 4}));

    EXPECT_EQ(buffer.pop(), 2);
    EXPECT_EQ(buffer.size(), 2U);
    buffer[0] = 30;
    EXPECT_EQ(buffer.at(0), 30);
    EXPECT_THROW((void)buffer.at(2), std::out_of_range);

    buffer.clear();
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.capacity(), 3U);
    EXPECT_THROW((void)buffer.pop(), std::out_of_range);
    EXPECT_THROW(RingBuffer<int>(0), std::invalid_argument);
}

} // namespace haylen::core
