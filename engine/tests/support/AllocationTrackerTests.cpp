#include <gtest/gtest.h>

#include <new>

#include "support/AllocationTracker.hpp"

namespace haylen::test {

// The test calls the allocation functions themselves, since an optimizing compiler may leave out the allocation of a new-expression whose storage nothing reads.
TEST(AllocationTrackerTest, CountsEveryFormOfAllocation) {
    const AllocationTracker tracker;
    void* single = ::operator new(sizeof(int));
    void* array = ::operator new[](64 * sizeof(int));
    void* quiet = ::operator new(sizeof(int), std::nothrow);
    void* quietArray = ::operator new[](128 * sizeof(int), std::nothrow);
    EXPECT_EQ(tracker.getLargest(), 128 * sizeof(int));
    EXPECT_EQ(tracker.getTotal(), (2 + 64 + 128) * sizeof(int));

    ::operator delete[](quietArray, std::nothrow);
    ::operator delete(quiet, std::nothrow);
    ::operator delete[](array);
    ::operator delete(single);
}

} // namespace haylen::test
