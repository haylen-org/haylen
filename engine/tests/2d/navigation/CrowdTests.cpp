#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "haylen/2d/navigation/Crowd.hpp"
#include "haylen/2d/navigation/SteeringAgent.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Geometry.hpp"
#include "support/EngineFixture.hpp"

namespace haylen {

namespace {

// Places agents on a circle, each heading for the opposite side, which makes them all meet in the middle.
std::vector<std::uint32_t> swapAcrossCircle(navigation2d::Crowd& crowd, int count, float radius) {
    std::vector<std::uint32_t> ids;
    for (int index = 0; index < count; ++index) {
        const float angle = std::numbers::pi_v<float> * 2.0F * static_cast<float>(index) / static_cast<float>(count);
        const math::Vec2 start = math::Vec2::fromAngle(angle, radius);
        ids.push_back(crowd.addAgent({.position = start, .radius = 12.0F, .maxSpeed = 60.0F, .neighborDistance = 90.0F}));
        crowd.setTarget(ids.back(), -start);
    }
    return ids;
}

float closestPair(const navigation2d::Crowd& crowd, const std::vector<std::uint32_t>& ids) {
    float closest = std::numeric_limits<float>::infinity();
    for (std::size_t first = 0; first < ids.size(); ++first) {
        for (std::size_t second = first + 1; second < ids.size(); ++second) {
            const float gap = math::Vec2::distance(crowd.getPosition(ids[first]), crowd.getPosition(ids[second])) - crowd.getRadius(ids[first]) - crowd.getRadius(ids[second]);
            closest = std::min(closest, gap);
        }
    }
    return closest;
}

} // namespace

TEST(CrowdTest, SwapsSidesWithoutAgentsOverlapping) {
    navigation2d::Crowd crowd;
    const std::vector<std::uint32_t> ring = swapAcrossCircle(crowd, 8, 200.0F);

    // Two columns cross each other head on, with their rows interleaved.
    std::vector<std::uint32_t> ids = ring;
    for (int row = 0; row < 10; ++row) {
        const float y = 600.0F + static_cast<float>(row) * 20.0F;
        ids.push_back(crowd.addAgent({.position = {-200.0F, y}, .radius = 8.0F, .maxSpeed = 60.0F}));
        crowd.setTarget(ids.back(), {200.0F, y});
        ids.push_back(crowd.addAgent({.position = {200.0F, y + 10.0F}, .radius = 8.0F, .maxSpeed = 60.0F}));
        crowd.setTarget(ids.back(), {-200.0F, y + 10.0F});
    }
    EXPECT_EQ(crowd.getAgentCount(), 28U);

    float closest = std::numeric_limits<float>::infinity();
    for (int step = 0; step < 600; ++step) {
        crowd.step(1.0F / 30.0F);
        closest = std::min(closest, closestPair(crowd, ids));
    }
    EXPECT_GT(closest, -0.5F);
    for (const std::uint32_t id : ids) {
        ASSERT_TRUE(crowd.getTarget(id).has_value());
        EXPECT_LT(math::Vec2::distance(crowd.getPosition(id), *crowd.getTarget(id)), 2.0F);
        EXPECT_LE(crowd.getVelocity(id).getLength(), crowd.getMaxSpeed(id) + 1e-3F);
    }
}

TEST(CrowdTest, WalksAroundObstaclesAndWalls) {
    navigation2d::Crowd crowd;
    const std::vector<math::Vec2> block{{-40.0F, -40.0F}, {-40.0F, 40.0F}, {40.0F, 40.0F}, {40.0F, -40.0F}};
    crowd.addObstacle(block);
    crowd.addObstacle(std::vector<math::Vec2>{{100.0F, -200.0F}, {100.0F, 200.0F}});
    const std::uint32_t walker = crowd.addAgent({.position = {-150.0F, 45.0F}, .radius = 10.0F, .maxSpeed = 80.0F});
    crowd.setTarget(walker, {60.0F, 60.0F});

    for (int step = 0; step < 400; ++step) {
        crowd.step(1.0F / 30.0F);
        const math::Vec2 position = crowd.getPosition(walker);
        EXPECT_FALSE(math::Geometry::contains(block, position));
        EXPECT_LT(position.x, 100.0F);
    }
    EXPECT_LT(math::Vec2::distance(crowd.getPosition(walker), {60.0F, 60.0F}), 2.0F);

    // A preferred velocity replaces the target, and the wall stops the agent that runs into it.
    crowd.setPreferredVelocity(walker, {80.0F, 0.0F});
    EXPECT_FALSE(crowd.getTarget(walker).has_value());
    EXPECT_EQ(crowd.getPreferredVelocity(walker), (math::Vec2{80.0F, 0.0F}));
    for (int step = 0; step < 200; ++step) {
        crowd.step(1.0F / 30.0F);
    }
    EXPECT_LT(crowd.getPosition(walker).x, 100.0F - 10.0F + 0.5F);

    crowd.clearObstacles();
    for (int step = 0; step < 100; ++step) {
        crowd.step(1.0F / 30.0F);
    }
    EXPECT_GT(crowd.getPosition(walker).x, 150.0F);
}

TEST(CrowdTest, StepsTheSameWayOnEveryThread) {
    test::EngineFixture fixture;
    navigation2d::Crowd serial;
    navigation2d::Crowd parallel;
    const std::vector<std::uint32_t> ids = swapAcrossCircle(serial, 300, 1500.0F);
    swapAcrossCircle(parallel, 300, 1500.0F);
    for (int step = 0; step < 60; ++step) {
        serial.step(1.0F / 30.0F);
        parallel.step(1.0F / 30.0F, &fixture.engine().getJobs());
    }
    for (const std::uint32_t id : ids) {
        EXPECT_EQ(serial.getPosition(id), parallel.getPosition(id));
    }
}

TEST(CrowdTest, FlocksAndManagesAgents) {
    navigation2d::Crowd crowd({.separation = 0.0F, .alignment = 0.0F, .cohesion = 1.0F});
    const std::uint32_t left = crowd.addAgent({.position = {-60.0F, 0.0F}, .radius = 5.0F, .maxSpeed = 40.0F});
    const std::uint32_t right = crowd.addAgent({.position = {60.0F, 0.0F}, .radius = 5.0F, .maxSpeed = 40.0F});
    for (int step = 0; step < 60; ++step) {
        crowd.step(1.0F / 30.0F);
    }
    EXPECT_LT(math::Vec2::distance(crowd.getPosition(left), crowd.getPosition(right)), 100.0F);
    EXPECT_EQ(crowd.getFlocking().cohesion, 1.0F);
    crowd.setFlocking({.separation = 2.0F});
    EXPECT_EQ(crowd.getFlocking().separation, 2.0F);

    EXPECT_TRUE(crowd.removeAgent(left));
    EXPECT_FALSE(crowd.removeAgent(left));
    EXPECT_FALSE(crowd.hasAgent(left));
    EXPECT_EQ(crowd.getAgentCount(), 1U);
    EXPECT_EQ(crowd.addAgent({}), left);
    crowd.setPosition(left, {5.0F, 6.0F});
    EXPECT_EQ(crowd.getPosition(left), (math::Vec2{5.0F, 6.0F}));
    crowd.clearTarget(left);

    EXPECT_THROW((void)crowd.getPosition(99), std::out_of_range);
    EXPECT_THROW((void)crowd.addAgent({.radius = -1.0F}), std::invalid_argument);
    EXPECT_THROW(crowd.addObstacle(std::vector<math::Vec2>{{0.0F, 0.0F}}), std::invalid_argument);
    EXPECT_THROW(crowd.step(0.0F), std::invalid_argument);
}

TEST(SteeringTest, AlignsCoheresAndAvoidsObstacles) {
    const navigation2d::SteeringAgent agent{.position = {0.0F, 0.0F}, .velocity = {10.0F, 0.0F}, .maxSpeed = 100.0F, .maxForce = 50.0F};
    const std::vector<math::Vec2> headings{{0.0F, 30.0F}, {0.0F, 10.0F}};
    EXPECT_EQ(agent.alignment(headings), math::Vec2(-10.0F, 100.0F));
    EXPECT_EQ(agent.alignment({}), math::Vec2());
    const std::vector<math::Vec2> flock{{20.0F, 0.0F}, {40.0F, 0.0F}};
    EXPECT_EQ(agent.cohesion(flock), math::Vec2(90.0F, 0.0F));
    EXPECT_EQ(agent.cohesion({}), math::Vec2());

    // A rock slightly left of the path pushes the agent to the other side, harder when it is closer.
    const std::vector<math::Circle> rocks{{{50.0F, -5.0F}, 10.0F}, {{200.0F, 0.0F}, 10.0F}};
    const math::Vec2 swerve = agent.avoid(rocks, 100.0F);
    EXPECT_EQ(swerve.x, 0.0F);
    EXPECT_GT(swerve.y, 0.0F);
    EXPECT_GT(swerve.y, agent.avoid(std::vector<math::Circle>{{{80.0F, -5.0F}, 10.0F}}, 100.0F).y);
    EXPECT_EQ(agent.avoid(rocks, 20.0F), math::Vec2());
    const navigation2d::SteeringAgent resting{.position = {}, .velocity = {}, .maxSpeed = 100.0F, .maxForce = 50.0F};
    EXPECT_EQ(resting.avoid(rocks, 100.0F), math::Vec2());
    EXPECT_NE(agent.avoid(std::vector<math::Circle>{{{50.0F, 0.0F}, 10.0F}}, 100.0F), math::Vec2());
}

} // namespace haylen
