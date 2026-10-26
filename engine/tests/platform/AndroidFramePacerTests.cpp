#include <gtest/gtest.h>

#include "platform/android/AndroidFramePacer.hpp"

namespace haylen::platform {

TEST(AndroidFramePacerTest, DrawsOnlyWhileResumedWithAWindow) {
    AndroidFramePacer pacer;
    pacer.setResumed(true);
    EXPECT_FALSE(pacer.isDrawing());

    pacer.setWindow(true);
    EXPECT_TRUE(pacer.isDrawing());

    pacer.setResumed(false);
    EXPECT_FALSE(pacer.isDrawing());
}

TEST(AndroidFramePacerTest, TicksAStartedAppWhilePaused) {
    AndroidFramePacer pacer;
    EXPECT_FALSE(pacer.takeTick(1.0));
    EXPECT_EQ(pacer.getWaitMilliseconds(1.0), -1);

    pacer.setStarted(true);
    EXPECT_TRUE(pacer.takeTick(1.0));
    EXPECT_FALSE(pacer.takeTick(1.05));
    EXPECT_NEAR(pacer.getWaitMilliseconds(1.05), 50, 1);
    EXPECT_TRUE(pacer.takeTick(1.1));

    // A resumed app draws its frames instead, and an app that ended ticks no more.
    pacer.setResumed(true);
    EXPECT_FALSE(pacer.takeTick(2.0));
    EXPECT_EQ(pacer.getWaitMilliseconds(2.0), -1);
    pacer.setResumed(false);
    pacer.setStarted(false);
    EXPECT_FALSE(pacer.takeTick(3.0));
}

TEST(AndroidFramePacerTest, CountsTheSecondsSinceTheFrameBefore) {
    AndroidFramePacer pacer;
    EXPECT_DOUBLE_EQ(pacer.takeElapsed(5.0), 0.0);
    EXPECT_DOUBLE_EQ(pacer.takeElapsed(5.25), 0.25);
    EXPECT_DOUBLE_EQ(pacer.takeElapsed(5.25), 0.0);
}

} // namespace haylen::platform
