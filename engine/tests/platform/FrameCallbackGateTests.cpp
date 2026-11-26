#include <gtest/gtest.h>

#include "platform/FrameCallbackGate.hpp"

namespace haylen::platform {

// The frames of a window follow the callbacks of the window system while they come and a timer once the window system holds them, and a frame presents only once the callback of the last frame that presented arrived.
class FrameCallbackGateTest : public ::testing::Test {
  protected:
    static constexpr double kTick = 1.0 / 60.0;

    FrameCallbackGate gate;
};

TEST_F(FrameCallbackGateTest, PresentsAtEveryCallbackWithoutFramesOfTheTimer) {
    double now = 0.0;
    for (int frame = 0; frame < 60; ++frame) {
        EXPECT_TRUE(gate.takePresent(now));
        now += kTick / 2.0;
        EXPECT_FALSE(gate.tick(now));
        now += kTick / 2.0;
        gate.receiveCallback();
    }
}

// A compositor that repaints at about 33 hertz, slower than the timer, still plays every frame at a callback, so the app updates as often as it presents.
TEST_F(FrameCallbackGateTest, PlaysEveryFrameAtACallbackOfASlowCompositor) {
    const double callbackPeriod = 1.0 / 33.0;
    double now = 0.0;
    double nextTick = kTick;
    int timerFrames = 0;
    for (int frame = 0; frame < 330; ++frame) {
        EXPECT_TRUE(gate.takePresent(now));
        const double callbackAt = now + callbackPeriod;
        for (; nextTick < callbackAt; nextTick += kTick) {
            timerFrames += gate.tick(nextTick) ? 1 : 0;
        }

        now = callbackAt;
        gate.receiveCallback();
    }

    EXPECT_EQ(timerFrames, 0);
}

TEST_F(FrameCallbackGateTest, PlaysFramesThatPresentNothingOnceTheCallbacksAreHeld) {
    EXPECT_TRUE(gate.takePresent(0.0));
    EXPECT_TRUE(gate.isWaiting());
    EXPECT_FALSE(gate.tick(FrameCallbackGate::kHeldSeconds / 2.0));

    for (double now = FrameCallbackGate::kHeldSeconds; now < 10.0; now += kTick) {
        EXPECT_TRUE(gate.tick(now));
        EXPECT_FALSE(gate.takePresent(now));
    }
}

TEST_F(FrameCallbackGateTest, PresentsAtOnceWhenTheCallbacksComeBack) {
    EXPECT_TRUE(gate.takePresent(0.0));
    EXPECT_TRUE(gate.tick(1.0));
    EXPECT_FALSE(gate.takePresent(1.0));

    gate.receiveCallback();
    EXPECT_FALSE(gate.isWaiting());
    EXPECT_TRUE(gate.takePresent(1.0));
    EXPECT_FALSE(gate.tick(1.0 + kTick));
}

TEST_F(FrameCallbackGateTest, PresentsNothingWhileTheWindowSystemSuspendsTheWindow) {
    gate.setSuspended(true);
    EXPECT_FALSE(gate.takePresent(0.0));
    EXPECT_TRUE(gate.tick(kTick));
    EXPECT_FALSE(gate.isWaiting());

    gate.setSuspended(false);
    EXPECT_TRUE(gate.takePresent(kTick));
}

} // namespace haylen::platform
