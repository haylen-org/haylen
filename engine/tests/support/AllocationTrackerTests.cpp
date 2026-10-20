#include <gtest/gtest.h>

#include <memory>
#include <new>

#include "support/AllocationTracker.hpp"

namespace haylen::test {

TEST(AllocationTrackerTest, CountsEveryFormOfAllocation) {
    const AllocationTracker tracker;
    const std::unique_ptr<int> single(new int(1));
    const std::unique_ptr<int[]> array(new int[64]);
    const std::unique_ptr<int> quiet(new (std::nothrow) int(2));
    const std::unique_ptr<int[]> quietArray(new (std::nothrow) int[128]);
    EXPECT_EQ(tracker.getLargest(), 128 * sizeof(int));
    EXPECT_EQ(tracker.getTotal(), (2 + 64 + 128) * sizeof(int));
}

} // namespace haylen::test
