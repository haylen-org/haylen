#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "haylen/2d/physics/PathPredictor.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/Engine.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::physics2d {

class WorldTuningTest : public ::testing::Test {
  protected:
    static void simulate(World& world, float seconds) {
        for (int step = 0; step < static_cast<int>(seconds * 60.0F); ++step) {
            world.step(1.0F / 60.0F);
        }
    }

    // Builds a column of boxes on a floor and returns them from the bottom up.
    static std::vector<Body> stack(World& world, int count) {
        Body floor = world.createBody({.type = Body::Type::Static, .position = {0.0F, 20.0F}});
        floor.addBox({1000.0F, 40.0F});
        std::vector<Body> boxes;
        for (int index = 0; index < count; ++index) {
            Body box = world.createBody({.position = {0.0F, -16.5F - static_cast<float>(index) * 32.5F}});
            box.addBox({32.0F, 32.0F});
            boxes.push_back(box);
        }
        return boxes;
    }
};

TEST_F(WorldTuningTest, StopsFastBodiesAtThinWalls) {
    // Continuous collision stops a fast body at a thin static wall and only a bullet at a thin dynamic wall.
    for (const bool continuous : {true, false}) {
        World world({.gravity = {}, .continuous = continuous});
        EXPECT_EQ(world.isContinuousEnabled(), continuous);
        Body wall = world.createBody({.type = Body::Type::Static, .position = {100.0F, 0.0F}});
        wall.addSegment({0.0F, -100.0F}, {0.0F, 100.0F});
        Body ball = world.createBody({.velocity = {20000.0F, 0.0F}});
        ball.addCircle(4.0F);
        world.step(1.0F / 60.0F);
        EXPECT_EQ(ball.getPosition().x < 100.0F, continuous);
    }

    for (const bool bullet : {false, true}) {
        World world({.gravity = {}});
        Body anchor = world.createBody({.type = Body::Type::Static, .position = {300.0F, -200.0F}});
        Body plate = world.createBody({.position = {300.0F, 0.0F}});
        plate.addBox({4.0F, 200.0F}, {.density = 20.0F});
        (void)world.createJoint(Joint::Type::Weld, anchor, plate, {.anchorA = {300.0F, -200.0F}});
        Body ball = world.createBody({.velocity = {6000.0F, 0.0F}, .bullet = bullet});
        ball.addCircle(4.0F);
        simulate(world, 0.2F);
        EXPECT_EQ(ball.getPosition().x < 300.0F, bullet);
    }
}

TEST_F(WorldTuningTest, ClampsSpeedsToTheWorldLimit) {
    World world({.gravity = {}});
    EXPECT_NEAR(world.getMaxSpeed(), 400.0F * 64.0F, 1.0F);
    Body ball = world.createBody({.velocity = {40000.0F, 0.0F}});
    ball.addCircle(4.0F);
    world.step(1.0F / 60.0F);
    EXPECT_NEAR(ball.getVelocity().x, 25600.0F, 1.0F);
    world.setMaxSpeed(60000.0F);
    ball.setVelocity({40000.0F, 0.0F});
    world.step(1.0F / 60.0F);
    EXPECT_NEAR(ball.getVelocity().x, 40000.0F, 1.0F);
    EXPECT_THROW(world.setMaxSpeed(-1.0F), std::invalid_argument);
}

TEST_F(WorldTuningTest, StacksSettleAndSleep) {
    World world;
    const std::vector<Body> boxes = stack(world, 10);
    simulate(world, 1.0F);
    const math::Vec2 settled = boxes.back().getPosition();
    simulate(world, 3.0F);
    EXPECT_EQ(world.getAwakeBodyCount(), 0U);
    EXPECT_LT(math::Vec2::distance(boxes.back().getPosition(), settled), 1.0F);

    // A sleeping pile reacts to a change of gravity, which wakes every body.
    world.setGravity({0.0F, -980.0F});
    EXPECT_EQ(world.getAwakeBodyCount(), 10U);
    world.step(1.0F / 60.0F);
    EXPECT_LT(boxes.back().getPosition().y, settled.y);

    // Without sleeping the pile stays awake, and more sub-steps cost more and hold more.
    World restless({.subSteps = 8, .sleepEnabled = false});
    (void)stack(restless, 5);
    simulate(restless, 4.0F);
    EXPECT_EQ(restless.getAwakeBodyCount(), 5U);
    EXPECT_EQ(restless.getSubSteps(), 8);
    restless.setSubSteps(2);
    EXPECT_EQ(restless.getSubSteps(), 2);
    EXPECT_THROW(restless.setSubSteps(0), std::invalid_argument);
    const World::Stats stats = restless.getStats();
    EXPECT_EQ(stats.bodies, 6);
    EXPECT_EQ(stats.awakeBodies, 5);
    EXPECT_GE(stats.contacts, 5);
}

TEST_F(WorldTuningTest, RestitutionAndHitThresholdsDecideSlowImpacts) {
    // A slow ball with full restitution stops dead under the threshold and bounces once the threshold is lowered.
    for (const float threshold : {64.0F, 1.0F}) {
        World world({.gravity = {}, .restitutionThreshold = threshold, .hitThreshold = threshold});
        EXPECT_NEAR(world.getRestitutionThreshold(), threshold, 1e-3F);
        Body wall = world.createBody({.type = Body::Type::Static, .position = {100.0F, 0.0F}});
        wall.addBox({20.0F, 200.0F});
        Body ball = world.createBody({.velocity = {40.0F, 0.0F}});
        ball.addCircle(8.0F, {.restitution = 1.0F});
        std::size_t hits = 0;
        for (int step = 0; step < 180; ++step) {
            world.step(1.0F / 60.0F);
            hits += world.getContactHits().size();
        }
        EXPECT_EQ(ball.getVelocity().x < -30.0F, threshold < 40.0F);
        EXPECT_EQ(hits > 0, threshold < 40.0F);
    }
}

TEST_F(WorldTuningTest, ShapesReportOnlyTheEventsTheyAskFor) {
    World world;
    Body floor = world.createBody({.type = Body::Type::Static, .position = {0.0F, 100.0F}});
    floor.addBox({1000.0F, 20.0F}, {.contactEvents = false, .hitEvents = false});
    Body quiet = world.createBody({.position = {-100.0F, 0.0F}});
    Shape quietShape = quiet.addBox({20.0F, 20.0F}, {.contactEvents = false, .hitEvents = false});
    Body loud = world.createBody({.position = {100.0F, 0.0F}});
    loud.addBox({20.0F, 20.0F});
    EXPECT_FALSE(quietShape.hasContactEvents());
    EXPECT_FALSE(quietShape.hasHitEvents());

    std::size_t begins = 0;
    std::size_t hits = 0;
    for (int step = 0; step < 60; ++step) {
        world.step(1.0F / 60.0F);
        begins += world.getContactBegins().size();
        hits += world.getContactHits().size();
    }
    EXPECT_EQ(begins, 1U);
    EXPECT_EQ(hits, 1U);
    quietShape.setHitEvents(true);
    EXPECT_TRUE(quietShape.hasHitEvents());
}

TEST_F(WorldTuningTest, InterpolatesBodiesBetweenSteps) {
    World world({.gravity = {}, .interpolate = true});
    EXPECT_TRUE(world.isInterpolating());
    Body body = world.createBody({.velocity = {60.0F, 0.0F}});
    body.addCircle(4.0F);
    world.step(1.0F / 60.0F);
    world.step(1.0F / 60.0F);
    EXPECT_NEAR(world.getInterpolatedTransform(body, 0.0F).position.x, 1.0F, 1e-3F);
    EXPECT_NEAR(world.getInterpolatedTransform(body, 0.5F).position.x, 1.5F, 1e-3F);
    std::vector<float> values(3);
    const std::vector<Body> bodies{body};
    world.readTransforms(bodies, values, 0.25F);
    EXPECT_NEAR(values[0], 1.25F, 1e-3F);

    // A teleported body draws where it is, without a blend from where it was.
    body.setTransform({500.0F, 0.0F}, 0.0F);
    EXPECT_NEAR(world.getInterpolatedTransform(body, 0.5F).position.x, 500.0F, 1e-3F);
    World plain;
    Body other = plain.createBody();
    EXPECT_THROW((void)plain.getInterpolatedTransform(other, 0.5F), std::logic_error);
}

TEST_F(WorldTuningTest, ThreadedStepsMatchSingleThreadedSteps) {
    // Box2D gives the same result on any number of threads, so two runs of a world large enough to use its threads hash alike.
    test::EngineFixture fixture;
    std::vector<std::uint64_t> hashes;
    for (const int threads : {1, 4}) {
        World world({.threads = threads}, &fixture.engine().getJobs());
        EXPECT_EQ(world.getThreads(), threads);
        Body floor = world.createBody({.type = Body::Type::Static, .position = {0.0F, 400.0F}});
        floor.addBox({3000.0F, 40.0F});
        std::vector<Body> boxes;
        for (int index = 0; index < World::kParallelBodies + 200; ++index) {
            Body box = world.createBody({.position = {static_cast<float>(index % 40) * 30.0F - 600.0F, 360.0F - static_cast<float>(index / 40) * 22.0F}, .rotation = static_cast<float>(index) * 0.1F});
            box.addBox({20.0F, 20.0F});
            boxes.push_back(box);
        }
        simulate(world, 1.0F);
        hashes.push_back(world.computeStateHash(boxes));
    }
    EXPECT_EQ(hashes[0], hashes[1]);
    EXPECT_THROW(World({.threads = 2}), std::invalid_argument);
    EXPECT_THROW(World({.threads = 0}), std::invalid_argument);
}

TEST_F(WorldTuningTest, PredictsTheFlightOfABody) {
    // The predicted points match the positions of a body thrown with the same speed, gravity scale and damping.
    World world;
    Body thrown = world.createBody({.position = {0.0F, 0.0F}, .velocity = {400.0F, -600.0F}, .linearDamping = 0.2F, .gravityScale = 0.8F});
    thrown.addCircle(4.0F, {.filter = {.category = 2, .mask = 0}});
    const PathPredictor::Path path = PathPredictor::predict(world, {0.0F, 0.0F}, {400.0F, -600.0F}, {.steps = 60, .gravityScale = 0.8F, .linearDamping = 0.2F});
    ASSERT_EQ(path.points.size(), 61U);
    for (std::size_t step = 1; step < path.points.size(); ++step) {
        world.step(1.0F / 60.0F);
        EXPECT_LT(math::Vec2::distance(path.points[step], thrown.getPosition()), 0.5F);
    }
    EXPECT_FALSE(path.hit.has_value());

    // A wall in the way ends the path at the hit.
    Body wall = world.createBody({.type = Body::Type::Static, .position = {300.0F, 0.0F}});
    wall.addBox({20.0F, 2000.0F});
    const PathPredictor::Path blocked = PathPredictor::predict(world, {0.0F, 0.0F}, {400.0F, -600.0F}, {.steps = 120, .radius = 4.0F});
    ASSERT_TRUE(blocked.hit.has_value());
    EXPECT_NEAR(blocked.points.back().x, 286.0F, 1.0F);
    EXPECT_THROW((void)PathPredictor::predict(world, {}, {}, {.steps = 0}), std::invalid_argument);
}

TEST_F(WorldTuningTest, PicksTheNearestShapesWithinARadius) {
    World world({.gravity = {}});
    Body rod = world.createBody({.position = {10.0F, 0.0F}});
    rod.addBox({4.0F, 100.0F});
    Body coin = world.createBody({.position = {-20.0F, 0.0F}});
    coin.addCircle(3.0F);
    EXPECT_TRUE(world.pick({0.0F, 0.0F}, 0.0F).empty());
    const std::vector<Shape> picked = world.pick({0.0F, 0.0F}, 25.0F);
    ASSERT_EQ(picked.size(), 2U);
    EXPECT_EQ(picked[0].getBody(), rod);
    EXPECT_EQ(picked[1].getBody(), coin);
}

} // namespace haylen::physics2d
