#include <gtest/gtest.h>

#include <cmath>

#include "ui/Scroller.hpp"

namespace haylen::ui {

namespace {

class ScrollerTest : public ::testing::Test {
  protected:
    using Motion = Context::WheelScroll::Motion;

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
    scroller.setRange(100000.0, 600.0);
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
    scroller.setRange(1000.0, 600.0);
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

TEST_F(ScrollerTest, StretchesLessTheFartherItIsPulledAndReturnsAlongTheSameCurve) {
    // A short pull stretches by about half its length, a long one by less for each unit, and the stretch never reaches the length of the view.
    scroller.setRange(1000.0, 600.0);
    scroller.drag(-20.0, kStep);
    const double first = -scroller.getOverscroll();
    EXPECT_GT(first, 10.0);
    EXPECT_LT(first, 11.0);
    for (int step = 0; step < 100; ++step) {
        scroller.drag(-20.0, kStep);
    }
    const double far = -scroller.getOverscroll();
    scroller.drag(-20.0, kStep);
    EXPECT_LT(-scroller.getOverscroll() - far, first / 4.0);
    EXPECT_LT(-scroller.getOverscroll(), 600.0);

    // Pulling back by the same distance brings the content back to its end.
    scroller.drag(20.0 * 102.0, kStep);
    EXPECT_NEAR(scroller.getOffset(), 0.0, 1e-6);
}

TEST_F(ScrollerTest, GlidesThroughTheStepsOfAWheel) {
    // Each step of a wheel moves the target, so steps that follow one another add up while the content glides there without passing it.
    scroller.setRange(5000.0, 600.0);
    scroller.follow({.distance = 64.0F, .motion = Motion::Glide}, 1.0, kStep);
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 0.0);
    scroller.update(kStep);
    scroller.follow({.distance = 64.0F, .motion = Motion::Glide}, 1.0, kStep);
    EXPECT_DOUBLE_EQ(scroller.getTarget(), 128.0);
    double offset = scroller.getOffset();
    while (!scroller.isResting()) {
        scroller.update(kStep);
        EXPECT_GE(scroller.getOffset(), offset);
        EXPECT_LE(scroller.getOffset(), 128.0);
        offset = scroller.getOffset();
    }
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 128.0);
}

// Fingers on a trackpad drag the content and stretch it past an end, and once they leave the content flings or springs back at once, as it does after a finger leaves the screen.
TEST_F(ScrollerTest, FollowsFingersOnATrackpadAndSpringsBackTheMomentTheyLeave) {
    scroller.setRange(1000.0, 600.0);
    scroller.follow({.distance = 30.0F, .motion = Motion::Drag}, 1.0, kStep);
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 30.0);
    scroller.follow({.distance = 1100.0F, .motion = Motion::Drag}, 1.0, kStep);
    EXPECT_GT(scroller.getOverscroll(), 0.0);

    // Fingers that rest keep the content stretched, and no motion runs until they leave.
    for (int step = 0; step < 30; ++step) {
        scroller.follow({.motion = Motion::Drag}, 1.0, kStep);
        scroller.update(kStep);
    }
    EXPECT_GT(scroller.getOverscroll(), 0.0);
    scroller.follow({.motion = Motion::Drag, .lifted = true}, 1.0, kStep);
    scroller.update(kStep);
    EXPECT_LT(scroller.getOverscroll(), 130.0);
    EXPECT_LT(settle(), 40);
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 1000.0);
}

TEST_F(ScrollerTest, FlingsWhenFingersLeaveATrackpadAndStopsWhenTheyTouchItAgain) {
    scroller.setRange(100000.0, 600.0);
    for (int step = 0; step < 6; ++step) {
        scroller.follow({.distance = 40.0F, .motion = Motion::Drag}, 1.0, kStep);
    }
    scroller.follow({.motion = Motion::Drag, .lifted = true}, 1.0, kStep);
    for (int step = 0; step < 10; ++step) {
        scroller.update(kStep);
    }
    const double flung = scroller.getOffset();
    EXPECT_GT(flung, 400.0);
    EXPECT_FALSE(scroller.isResting());
    scroller.follow({.touched = true}, 1.0, kStep);
    EXPECT_TRUE(scroller.isResting());
    scroller.update(kStep);
    EXPECT_DOUBLE_EQ(scroller.getOffset(), flung);
}

// A precise device that reports no phases moves the content within its range and never stretches it, since nothing tells when its fingers leave.
TEST_F(ScrollerTest, MovesThePointsOfADeviceWithoutPhasesWithinTheRange) {
    scroller.setRange(1000.0, 600.0);
    scroller.follow({.distance = 30.0F, .motion = Motion::Free}, 1.0, kStep);
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 30.0);
    scroller.follow({.distance = 2000.0F, .motion = Motion::Free}, 1.0, kStep);
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 1000.0);
    EXPECT_TRUE(scroller.isResting());
}

TEST_F(ScrollerTest, StopsAtTheEndsWhenItJumps) {
    scroller.setRange(500.0, 600.0);
    scroller.jumpTo(-40.0);
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 0.0);
    scroller.jumpTo(900.0);
    EXPECT_DOUBLE_EQ(scroller.getOffset(), 500.0);
    EXPECT_TRUE(scroller.isResting());
}

TEST_F(ScrollerTest, FollowsATargetThatMovesWithoutOvershooting) {
    scroller.setRange(5000.0, 600.0);
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
    scroller.setRange(5000.0, 600.0);
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
