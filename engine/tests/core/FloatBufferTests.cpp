#include <gtest/gtest.h>

#include <span>
#include <stdexcept>
#include <vector>

#include "haylen/core/FloatBuffer.hpp"

namespace haylen::core {

TEST(FloatBufferTest, ChecksBoundsAndSetsInBulk) {
    FloatBuffer buffer(4, 1.5F);
    EXPECT_EQ(buffer.size(), 4U);
    EXPECT_EQ(buffer.get(3), 1.5F);
    buffer.set(1, 7.0F);
    const std::vector<float> values{2.0F, 3.0F};
    buffer.set(2, values);
    EXPECT_EQ(std::vector<float>(buffer.getValues().begin(), buffer.getValues().end()), (std::vector<float>{1.5F, 7.0F, 2.0F, 3.0F}));
    EXPECT_THROW((void)buffer.get(4), std::out_of_range);
    EXPECT_THROW(buffer.set(4, 0.0F), std::out_of_range);
    EXPECT_THROW(buffer.set(3, values), std::out_of_range);
    EXPECT_THROW(buffer.set(5, std::span<const float>{}), std::out_of_range);
    buffer.fill(0.25F);
    EXPECT_EQ(buffer.get(0), 0.25F);
}

} // namespace haylen::core
