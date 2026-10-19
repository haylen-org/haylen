#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <string>

#include "haylen/2d/animation/Chain.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::animation2d {

class ChainTest : public ::testing::Test {
  protected:
    // Expects every segment to keep its length.
    static void expectLengths(const Chain& chain) {
        for (std::size_t index = 0; index < chain.getLengths().size(); ++index) {
            EXPECT_NEAR(math::Vec2::distance(chain.getPoints()[index], chain.getPoints()[index + 1]), chain.getLengths()[index], 1e-3F) << "Segment " << index;
        }
    }

    // Returns the largest bend between two segments in a row.
    static float largestBend(const Chain& chain) {
        float largest = 0.0F;
        for (std::size_t index = 2; index < chain.size(); ++index) {
            const math::Vec2 previous = chain.getPoints()[index - 1] - chain.getPoints()[index - 2];
            const math::Vec2 next = chain.getPoints()[index] - chain.getPoints()[index - 1];
            largest = std::max(largest, std::abs(std::atan2(math::Vec2::cross(previous, next), math::Vec2::dot(previous, next))));
        }
        return largest;
    }
};

TEST_F(ChainTest, ReachesATargetInReachAndKeepsItsRootAndLengths) {
    Chain arm({.root = {10.0F, 20.0F}, .lengths = {50.0F, 40.0F, 30.0F}});
    EXPECT_FLOAT_EQ(arm.getTotalLength(), 120.0F);
    EXPECT_NEAR(arm.getEnd().x, 130.0F, 1e-3F);

    EXPECT_TRUE(arm.reach({70.0F, 90.0F}, {.tolerance = 0.01F}));
    EXPECT_NEAR(math::Vec2::distance(arm.getEnd(), {70.0F, 90.0F}), 0.0F, 0.01F);
    EXPECT_EQ(arm.getRoot(), (math::Vec2{10.0F, 20.0F}));
    expectLengths(arm);
}

TEST_F(ChainTest, StretchesStraightTowardATargetOutOfReach) {
    Chain arm({.lengths = {30.0F, 30.0F}});
    EXPECT_FALSE(arm.reach({0.0F, 200.0F}, {}));
    EXPECT_NEAR(arm.getPoints()[1].y, 30.0F, 1e-4F);
    EXPECT_NEAR(arm.getEnd().x, 0.0F, 1e-4F);
    EXPECT_NEAR(arm.getEnd().y, 60.0F, 1e-4F);
    EXPECT_TRUE(arm.reach({0.0F, 60.0F}, {}));
}

TEST_F(ChainTest, BendsItsJointsTowardThePole) {
    // The leg starts straight, so the pole alone decides the side of the knee, and it changes sides when the pole does.
    Chain leg({.lengths = {50.0F, 50.0F}});
    EXPECT_TRUE(leg.reach({60.0F, 0.0F}, {.pole = math::Vec2{30.0F, -100.0F}}));
    EXPECT_LT(leg.getPoints()[1].y, -20.0F);
    expectLengths(leg);

    EXPECT_TRUE(leg.reach({60.0F, 0.0F}, {.pole = math::Vec2{30.0F, 100.0F}}));
    EXPECT_GT(leg.getPoints()[1].y, 20.0F);

    // Without a pole the knee stays on the side it bends to.
    EXPECT_TRUE(leg.reach({70.0F, 10.0F}, {}));
    EXPECT_GT(leg.getPoints()[1].y, 10.0F);
}

TEST_F(ChainTest, FollowsAHeadWithinTheBendLimit) {
    Chain body({.root = {0.0F, 0.0F}, .lengths = std::vector<float>(12, 10.0F), .angle = math::Math::kPi, .maxBend = 0.4F});
    for (int step = 0; step < 240; ++step) {
        const float turn = static_cast<float>(step) * 0.08F;
        body.follow({std::cos(turn) * 60.0F, std::sin(turn) * 60.0F});
        ASSERT_LE(largestBend(body), 0.4F + 1e-3F) << "Step " << step;
    }
    EXPECT_NEAR(body.getRoot().x, std::cos(239.0F * 0.08F) * 60.0F, 1e-4F);
    expectLengths(body);

    // A point pushed aside keeps its place until the next follow pulls the lengths back from the head.
    body.setPoint(5, {0.0F, 999.0F});
    EXPECT_EQ(body.getPoints()[5], (math::Vec2{0.0F, 999.0F}));
    body.follow(body.getRoot());
    expectLengths(body);
    EXPECT_THROW(body.setPoint(13, {}), std::out_of_range);

    body.setRoot({500.0F, 0.0F});
    EXPECT_NEAR(math::Vec2::distance(body.getRoot(), {500.0F, 0.0F}), 0.0F, 1e-4F);
    expectLengths(body);
}

TEST_F(ChainTest, ReachingAndFollowingNeverAllocate) {
    Chain tail({.lengths = std::vector<float>(30, 8.0F), .maxBend = 0.5F});
    const test::AllocationTracker tracker;
    for (int step = 0; step < 100; ++step) {
        (void)tail.reach({100.0F, static_cast<float>(step)}, {.pole = math::Vec2{0.0F, -50.0F}});
        tail.follow({static_cast<float>(step), 0.0F});
    }
    EXPECT_EQ(tracker.getTotal(), 0U);
}

TEST_F(ChainTest, RefusesChainsWithoutSegmentsOrWithEmptyOnes) {
    EXPECT_THROW(Chain({}), std::invalid_argument);
    EXPECT_THROW(Chain({.lengths = {10.0F, 0.0F}}), std::invalid_argument);
    EXPECT_THROW(Chain({.lengths = {NAN}}), std::invalid_argument);
}

TEST_F(ChainTest, BendsLegsAndBodiesFromLua) {
    test::EngineFixture fixture;
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local animation2d = require('haylen.animation2d')
        local collections = require('haylen.collections')
        local leg = animation2d.newChain({x = 0, y = 0, lengths = {50, 50}})
        local reached = leg:reach(60, 0, {pole = {30, -100}})
        local kneeX, kneeY = leg:point(2)
        local body = animation2d.newChain({segments = 4, length = 10, angle = math.pi, maxBend = 9})
        body:follow(5, 0)
        local buffer = collections.newFloatBuffer(12)
        body:writePoints(buffer, 3)
        body:setPoint(5, 0, 50)
        body:follow(5, 0)
        body.x = 105
        local endX = body:point(body.count)
        return string.format('%s %s %d %d %.0f %.0f %.0f %.2f %.0f %.0f', tostring(reached), tostring(kneeY < -20), leg.count, body.count, body.length, buffer[3], buffer[5], body.maxBend, endX, math.deg(leg:angle(2)) - math.deg(leg:angle(1)) > 0 and 1 or -1)
    )"), "true true 3 5 40 5 -5 3.14 79 1");
    // clang-format on
    EXPECT_EQ(fixture.lua("local animation2d = require('haylen.animation2d') local ok, message = pcall(animation2d.newChain, {lengths = {10}, segments = 2, length = 5}) return message:match('A chain.*')"), "A chain takes the lengths of its segments either as \"lengths\" or as \"segments\" of the same \"length\".");
    EXPECT_EQ(fixture.lua("local animation2d = require('haylen.animation2d') local ok, message = pcall(animation2d.newChain, {x = 1}) return message:match('A chain.*')"), "A chain takes the lengths of its segments either as \"lengths\" or as \"segments\" of the same \"length\".");
    EXPECT_EQ(fixture.lua("local animation2d = require('haylen.animation2d') local ok, message = pcall(animation2d.newChain, {segments = 0, length = 5}) return message:match('A chain.*')"), "A chain needs at least one segment in \"segments\".");
    EXPECT_EQ(fixture.lua("local animation2d = require('haylen.animation2d') local ok, message = pcall(animation2d.newChain, {segments = 2, length = -1}) return message:match('Every.*')"), "Every segment of a chain needs a finite length greater than zero.");
    EXPECT_EQ(fixture.lua("local animation2d = require('haylen.animation2d') local ok, message = pcall(animation2d.newChain, {lengths = {1}, width = 2}) return message:match('Unknown.*')"), "Unknown option \"width\".");
    EXPECT_EQ(fixture.lua("local animation2d = require('haylen.animation2d') local chain = animation2d.newChain({lengths = {1}}) local ok, message = pcall(chain.reach, chain, 1, 1, {steps = 2}) return message:match('Unknown.*')"), "Unknown option \"steps\".");
    EXPECT_EQ(fixture.lua("local animation2d = require('haylen.animation2d') local chain = animation2d.newChain({lengths = {1}}) local ok, message = pcall(chain.point, chain, 3) return message:match('points are.*')"), "points are numbered from one to the count of the chain)");
    EXPECT_EQ(fixture.lua("local animation2d = require('haylen.animation2d') local chain = animation2d.newChain({lengths = {1}}) local ok, message = pcall(chain.setPoint, chain, 0, 1, 1) return message:match('points are.*')"), "points are numbered from one to the count of the chain)");
    EXPECT_EQ(fixture.lua("local animation2d = require('haylen.animation2d') local collections = require('haylen.collections') local chain = animation2d.newChain({lengths = {1, 1}}) local ok, message = pcall(chain.writePoints, chain, collections.newFloatBuffer(6), 2) return message:match('The buffer.*')"), "The buffer holds 6 values, and the 3 points of the chain need 6 from value 2 on.");
}

} // namespace haylen::animation2d
