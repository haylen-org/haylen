#include <gtest/gtest.h>

#include <cmath>

#include "ui/Scroller.hpp"

namespace haylen::ui {

namespace {

class ScrollerTest : public ::testing::Test {
  protected:
    static constexpr float kStep = 1.0F / 60.0F;

    // Runs the scroller until it rests and returns the steps it took.
    int settle() {
        int steps = 0;
        while (!scroller.isResting() && steps < 2000) {
            scroller.update(kStep);
            ++steps;
        }
        return steps;
    }

    Scroller scroller;
};

} // namespace

TEST_F(ScrollerTest, FlingsAndComesToRest) {
    scroller.setMaximum(100000.0);
    for (int step = 0; step < 10; ++step) {
        scroller.drag(50.0, kStep);
    }
    EXPECT_NEAR(scroller.getVelocity(), 3000.0, 1.0);
    scroller.release();

    double offset = scroller.getOffset();
    double speed = scroller.getVelocity();
    while (!scroller.isResting()) {
        ASSERT_TRUE(scroller.update(kStep));
        EXPECT_GT(scroller.getOffset(), offset);
        EXPECT_LE(std::fabs(scroller.getVelocity()), speed);
        offset = scroller.getOffset();
        speed = std::fabs(scroller.getVelocity());
    }
    EXPECT_GT(scroller.getOffset(), 1000.0);
    EXPECT_LT(scroller.getOffset(), 100000.0);
}

TEST_F(ScrollerTest, SpringsBackFromBeyondTheEnds) {
    scroller.setMaximum(1000.0);
    for (int step = 0; step < 4; ++step) {
        scroller.drag(-50.0, kStep);
    }
    EXPECT_LT(scroller.getOverscroll(), 0.0);
    EXPECT_GT(scroller.getOverscroll(), -200.0);

    scroller.release();
    EXPECT_GT(settle(), 0);
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 0.0);

    scroller.jumpTo(1000.0);
    scroller.drag(120.0, kStep);
    scroller.release();
    settle();
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 1000.0);
}

TEST_F(ScrollerTest, StopsAtTheEndsWhenItJumps) {
    scroller.setMaximum(500.0);
    scroller.jumpTo(-40.0);
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 0.0);
    scroller.jumpTo(900.0);
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 500.0);
    EXPECT_TRUE(scroller.isResting());
}

TEST_F(ScrollerTest, FollowsATargetThatMovesWithoutOvershooting) {
    scroller.setMaximum(5000.0);
    scroller.animateTo(500.0);
    for (int step = 0; step < 5; ++step) {
        scroller.update(kStep);
    }
    EXPECT_GT(scroller.getOffset(), 0.0);
    EXPECT_LT(scroller.getOffset(), 500.0);

    scroller.animateTo(800.0);
    double offset = scroller.getOffset();
    while (!scroller.isResting()) {
        scroller.update(kStep);
        EXPECT_GE(scroller.getOffset(), offset);
        EXPECT_LE(scroller.getOffset(), 800.0);
        offset = scroller.getOffset();
    }
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 800.0);
}

TEST_F(ScrollerTest, ShiftsTheOffsetAndTheTargetTogether) {
    scroller.setMaximum(5000.0);
    scroller.animateTo(1000.0);
    scroller.update(kStep);
    const double before = scroller.getOffset();
    scroller.shift(200.0);
    EXPECT_DOUBLE_EQ(scroller.getOffset(), before + 200.0);
    EXPECT_DOUBLE_EQ(scroller.getTarget(), 1200.0);
    EXPECT_TRUE(scroller.isAnimating());
    settle();
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 1200.0);
}

} // namespace haylen::ui
