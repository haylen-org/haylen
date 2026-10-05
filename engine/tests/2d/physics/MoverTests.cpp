#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "haylen/2d/physics/Mover.hpp"
#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

class MoverTest : public ::testing::Test {
  protected:
    // Runs the steps an app runs: gravity into the velocity, the move, the velocity clipped against what the mover hit, and the world step.
    void walk(Mover& mover, float speed, float seconds) {
        for (int step = 0; step < static_cast<int>(seconds * 60.0F); ++step) {
            velocity = mover.clip({speed, velocity.y + 980.0F / 60.0F});
            mover.move(velocity / 60.0F);
            velocity = mover.clip(velocity);
            world.step(1.0F / 60.0F);
        }
    }

    Body floor(math::Vec2 center, math::Vec2 size, float rotation = 0.0F) {
        Body body = world.createBody({.type = Body::Type::Static, .position = center, .rotation = rotation});
        body.addBox(size);
        return body;
    }

    World world;
    math::Vec2 velocity{};
};

TEST_F(MoverTest, StandsAndWalksOnTheGround) {
    floor({0.0F, 100.0F}, {2000.0F, 20.0F});
    Mover mover(world, {.position = {0.0F, 0.0F}, .radius = 12.0F, .height = 48.0F});
    walk(mover, 0.0F, 1.0F);
    EXPECT_TRUE(mover.isGrounded());
    EXPECT_NEAR(mover.getPosition().y, 66.0F, 1.0F);
    EXPECT_NEAR(mover.getGroundNormal().y, -1.0F, 1e-3F);

    walk(mover, 300.0F, 1.0F);
    EXPECT_NEAR(mover.getPosition().x, 300.0F, 10.0F);
    EXPECT_TRUE(mover.isGrounded());
    EXPECT_NEAR(mover.getBody().getPosition().x, mover.getPosition().x, 6.0F);

    // Once the mover stands still its body stops with it instead of drifting on.
    walk(mover, 0.0F, 1.0F);
    EXPECT_NEAR(mover.getBody().getPosition().x, mover.getPosition().x, 0.01F);
    EXPECT_THROW(Mover(world, {.radius = 0.0F}), std::invalid_argument);
    EXPECT_THROW(Mover(world, {.radius = 20.0F, .height = 30.0F}), std::invalid_argument);
}

TEST_F(MoverTest, SlidesAlongWallsAndClimbsSteps) {
    floor({0.0F, 100.0F}, {2000.0F, 20.0F});
    floor({300.0F, 82.0F}, {200.0F, 16.0F});
    floor({-300.0F, 40.0F}, {40.0F, 100.0F});
    Mover mover(world, {.position = {0.0F, 60.0F}, .stepHeight = 20.0F});
    walk(mover, 0.0F, 0.5F);

    // A step lower than the step height is climbed, and a wall stops the mover.
    walk(mover, 300.0F, 0.9F);
    EXPECT_GT(mover.getPosition().x, 250.0F);
    EXPECT_NEAR(mover.getPosition().y, 50.0F, 1.0F);
    walk(mover, -500.0F, 2.5F);
    EXPECT_NEAR(mover.getPosition().x, -268.0F, 2.0F);
    EXPECT_TRUE(mover.isOnWall());
    EXPECT_TRUE(mover.isGrounded());
}

TEST_F(MoverTest, StandsOnSlopesUnderItsLimit) {
    // Idle on a 30 degree ramp the mover stays put, and on a 60 degree ramp it slides down.
    const float gentle = std::numbers::pi_v<float> / 6.0F;
    const float steep = std::numbers::pi_v<float> / 3.0F;
    floor({0.0F, 0.0F}, {600.0F, 20.0F}, gentle);
    floor({2000.0F, 0.0F}, {600.0F, 20.0F}, steep);
    Mover standing(world, {.position = {0.0F, -60.0F}});
    Mover sliding(world, {.position = {2000.0F, -80.0F}});
    walk(standing, 0.0F, 0.5F);
    const math::Vec2 rest = standing.getPosition();
    walk(standing, 0.0F, 2.0F);
    EXPECT_TRUE(standing.isGrounded());
    EXPECT_LT(math::Vec2::distance(standing.getPosition(), rest), 1.0F);

    velocity = {};
    const math::Vec2 start = sliding.getPosition();
    walk(sliding, 0.0F, 1.0F);
    EXPECT_GT(sliding.getPosition().y, start.y + 50.0F);
    EXPECT_FALSE(sliding.isGrounded());
}

TEST_F(MoverTest, SnapsToTheGroundOverCrests) {
    // Running over a crest and down a 30 degree slope keeps the mover on the ground at every step.
    const float slope = std::numbers::pi_v<float> / 6.0F;
    Body hill = world.createBody({.type = Body::Type::Static});
    hill.addChain(std::vector<math::Vec2>{{-600.0F, 0.0F}, {0.0F, 0.0F}, {600.0F * std::cos(slope), 600.0F * std::sin(slope)}}, false);
    Mover mover(world, {.position = {-300.0F, -24.0F}, .snapDistance = 12.0F});
    walk(mover, 0.0F, 0.3F);
    int airborne = 0;
    for (int step = 0; step < 90; ++step) {
        walk(mover, 400.0F, 1.0F / 60.0F);
        airborne += mover.isGrounded() ? 0 : 1;
    }
    EXPECT_EQ(airborne, 0);
    EXPECT_GT(mover.getPosition().x, 200.0F);
}

TEST_F(MoverTest, RidesMovingPlatforms) {
    // A mover standing on a kinematic platform keeps its place on it while the platform moves.
    Body platform = world.createBody({.type = Body::Type::Kinematic, .position = {0.0F, 100.0F}, .velocity = {100.0F, -40.0F}});
    platform.addBox({300.0F, 20.0F});
    Mover mover(world, {.position = {0.0F, 60.0F}});
    walk(mover, 0.0F, 0.2F);
    const math::Vec2 offset = mover.getPosition() - platform.getPosition();
    walk(mover, 0.0F, 2.0F);
    EXPECT_LT(math::Vec2::distance(mover.getPosition() - platform.getPosition(), offset), 2.0F);
    EXPECT_TRUE(mover.getGroundBody().has_value());
    EXPECT_NEAR(mover.getGroundVelocity().x, 100.0F, 1.0F);
}

TEST_F(MoverTest, PushesCratesAndStopsAtCeilings) {
    floor({0.0F, 100.0F}, {2000.0F, 20.0F});
    Body crate = world.createBody({.position = {80.0F, 74.0F}});
    crate.addBox({32.0F, 32.0F}, {.friction = 0.3F});
    Mover mover(world, {.position = {0.0F, 60.0F}});
    walk(mover, 200.0F, 1.5F);
    EXPECT_GT(crate.getPosition().x, 200.0F);

    // A jump under a low ceiling stops there and the mover falls back.
    floor({-400.0F, 0.0F}, {200.0F, 20.0F});
    mover.setPosition({-400.0F, 60.0F});
    velocity = {0.0F, -600.0F};
    bool bumped = false;
    for (int step = 0; step < 30; ++step) {
        mover.move(velocity / 60.0F);
        bumped = bumped || mover.isOnCeiling();
        velocity = mover.clip(velocity + math::Vec2{0.0F, 980.0F / 60.0F});
        world.step(1.0F / 60.0F);
    }
    EXPECT_TRUE(bumped);
    EXPECT_GT(mover.getPosition().y, 30.0F);
}

TEST_F(MoverTest, JumpsUpThroughOneWayPlatformsAndDropsDown) {
    // A mover jumps up through a one-way platform from below, lands on it and drops through it back to the floor.
    floor({0.0F, 100.0F}, {2000.0F, 20.0F});
    Body ledge = world.createBody({.type = Body::Type::Static, .position = {0.0F, 20.0F}});
    ledge.addBox({200.0F, 10.0F}, {.oneWay = math::Vec2{0.0F, -1.0F}});
    Mover mover(world, {.position = {0.0F, 66.0F}});
    walk(mover, 0.0F, 0.2F);
    ASSERT_TRUE(mover.isGrounded());
    velocity = {0.0F, -700.0F};
    for (int step = 0; step < 90; ++step) {
        mover.move(velocity / 60.0F);
        velocity = mover.clip(velocity + math::Vec2{0.0F, 980.0F / 60.0F});
        world.step(1.0F / 60.0F);
    }
    EXPECT_TRUE(mover.isGrounded());
    EXPECT_NEAR(mover.getPosition().y, -9.0F, 1.0F);
    ASSERT_TRUE(mover.getGroundBody().has_value());
    EXPECT_EQ(*mover.getGroundBody(), ledge);

    mover.dropThrough(0.2F);
    walk(mover, 0.0F, 1.0F);
    EXPECT_NEAR(mover.getPosition().y, 66.0F, 1.0F);
    EXPECT_THROW(mover.dropThrough(-1.0F), std::invalid_argument);
}

} // namespace haylen::physics2d
