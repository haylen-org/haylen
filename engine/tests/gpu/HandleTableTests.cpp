#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "gpu/HandleTable.hpp"
#include "haylen/gpu/ContractError.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::gpu {

// A slot that is taken again and again, as the render target of every frame takes the slot its predecessor left, keeps every earlier id away from its record, well past the 4096 reuses after which a generation of 12 bits wrapped.
TEST(HandleTableTest, KeepsTheIdsOfDestroyedRecordsAwayFromTheirSlotPastManyReuses) {
    HandleTable<std::string> table("image");
    const std::uint64_t first = table.add("first");
    (void)table.remove(first);

    std::uint64_t latest = 0;
    for (int reuse = 0; reuse < 5000; ++reuse) {
        latest = table.add("next");
        ASSERT_EQ(table.find(first), nullptr);
        (void)table.remove(latest);
    }

    latest = table.add("last");
    EXPECT_EQ(table.capacity(), 1);
    EXPECT_EQ(table.find(first), nullptr);
    ASSERT_NE(table.find(latest), nullptr);
    EXPECT_EQ(*table.find(latest), "last");
}

TEST(HandleTableTest, NamesNoRecordWithIdZeroOrAnIdItNeverGave) {
    HandleTable<int> table("buffer");
    const std::uint64_t id = table.add(7);

    EXPECT_NE(id, 0U);
    EXPECT_EQ(table.find(0), nullptr);
    EXPECT_EQ(table.find(id + 1), nullptr);
    EXPECT_FALSE(table.remove(0).has_value());

    try {
        (void)table.get(id + 1);
        FAIL() << "A missing record was found.";
    } catch (const ContractError& error) {
        EXPECT_EQ(error.getRule(), ContractError::Rule::MissingObject);
        EXPECT_STREQ(error.what(), "Haylen hit an internal error in its graphics device. Report it with the steps that led to it.");
    }
}

// The device clears its tables when it shuts down, and an id from before never names a record made after.
TEST(HandleTableTest, KeepsTheGenerationsOfItsSlotsThroughAClear) {
    HandleTable<int> table("sampler");
    const std::uint64_t before = table.add(1);
    table.clear();

    EXPECT_EQ(table.size(), 0);
    const std::uint64_t after = table.add(2);
    EXPECT_NE(after, before);
    EXPECT_EQ(table.find(before), nullptr);
    ASSERT_NE(table.find(after), nullptr);
    EXPECT_EQ(*table.find(after), 2);
}

// The destroys of a device take records out of their tables and must not fail, so taking out every record allocates nothing, whatever order the records leave in.
TEST(HandleTableTest, RemovesRecordsWithoutAllocating) {
    HandleTable<int> table("pipeline");
    std::vector<std::uint64_t> ids;
    for (int record = 0; record < 100; ++record) {
        ids.push_back(table.add(int{record}));
    }

    const test::AllocationTracker tracker;
    for (auto id = ids.rbegin(); id != ids.rend(); ++id) {
        EXPECT_TRUE(table.remove(*id).has_value());
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_EQ(table.size(), 0);
}

} // namespace haylen::gpu
