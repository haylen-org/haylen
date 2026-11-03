#include <gtest/gtest.h>

#include "platform/FrameRateGate.hpp"

namespace haylen::platform {

namespace {

// Counts the frames that the gate plays over a second of refreshes of the display at the rate in hertz, each a little early or late by turns.
class Refreshes final {
  public:
    static int countFrames(FrameRateGate& gate, double hertz) {
        int frames = 0;
        for (int refresh = 0; refresh < static_cast<int>(hertz); ++refresh) {
            const double jitter = refresh % 2 == 0 ? 0.0005 : -0.0005;
            frames += gate.takeFrame(1.0 + refresh / hertz + jitter) ? 1 : 0;
        }
        return frames;
    }
};

} // namespace

TEST(FrameRateGateTest, PlaysEveryRefreshWithoutATarget) {
    FrameRateGate gate;
    EXPECT_EQ(Refreshes::countFrames(gate, 120.0), 120);
}

TEST(FrameRateGateTest, PlaysAWholeNumberOfRefreshesApartForATargetBelowTheDisplay) {
    FrameRateGate gate;
    gate.setTarget(30);
    EXPECT_EQ(Refreshes::countFrames(gate, 60.0), 30);

    gate.setTarget(60);
    EXPECT_EQ(Refreshes::countFrames(gate, 120.0), 60);

    gate.setTarget(30);
    EXPECT_EQ(Refreshes::countFrames(gate, 120.0), 30);
}

TEST(FrameRateGateTest, AveragesATargetThatTheDisplayDoesNotDivide) {
    FrameRateGate gate;
    gate.setTarget(60);
    EXPECT_NEAR(Refreshes::countFrames(gate, 144.0), 60, 1);
}

TEST(FrameRateGateTest, PlaysEveryRefreshOfADisplaySlowerThanTheTarget) {
    FrameRateGate gate;
    gate.setTarget(120);
    EXPECT_EQ(Refreshes::countFrames(gate, 60.0), 60);
}

TEST(FrameRateGateTest, TellsHowLongALoopWaitsForTheNextFrame) {
    FrameRateGate gate;
    EXPECT_EQ(gate.getWaitSeconds(1.0), 0.0);

    gate.setTarget(20);
    EXPECT_EQ(gate.getWaitSeconds(1.0), 0.0);
    EXPECT_TRUE(gate.takeFrame(1.0));
    EXPECT_NEAR(gate.getWaitSeconds(1.01), 0.0275, 1.0e-9);
    EXPECT_EQ(gate.getWaitSeconds(1.2), 0.0);
}

TEST(FrameRateGateTest, StartsTheBeatAgainAfterALateFrame) {
    FrameRateGate gate;
    gate.setTarget(30);
    EXPECT_TRUE(gate.takeFrame(1.0));
    EXPECT_TRUE(gate.takeFrame(1.5));
    EXPECT_FALSE(gate.takeFrame(1.52));
    EXPECT_TRUE(gate.takeFrame(1.5 + 1.0 / 30.0));
}

} // namespace haylen::platform
