#include <gtest/gtest.h>

#include <cstdint>

#include "platform/sdl/FrameTimer.hpp"

namespace haylen::platform::sdl {

class FrameTimerTest : public ::testing::Test {
  protected:
    static constexpr std::uint64_t kMillisecond = 1'000'000;
};

TEST_F(FrameTimerTest, SmoothsFramesThatTheDisplayPaces) {
    FrameTimer timer;
    std::uint64_t now = 1000 * kMillisecond;
    EXPECT_DOUBLE_EQ(timer.next(now), 1.0 / 60.0);

    now += 17 * kMillisecond;
    const double first = timer.next(now);
    now += 16 * kMillisecond;
    const double second = timer.next(now);
    EXPECT_GT(first, 0.016);
    EXPECT_LT(first, 0.0171);
    EXPECT_GT(second, 0.016);
    EXPECT_LT(second, first);
}

TEST_F(FrameTimerTest, StartsAgainFromAFrameThatTakesClearlyLonger) {
    FrameTimer timer;
    std::uint64_t now = kMillisecond;
    (void)timer.next(now);
    now += 16 * kMillisecond;
    (void)timer.next(now);
    now += 33 * kMillisecond;
    EXPECT_DOUBLE_EQ(timer.next(now), 0.033);
}

TEST_F(FrameTimerTest, LimitsAFrameThatTookVeryLong) {
    FrameTimer timer;
    (void)timer.next(kMillisecond);
    EXPECT_DOUBLE_EQ(timer.next(kMillisecond + 2000 * kMillisecond), 0.1);
}

} // namespace haylen::platform::sdl
