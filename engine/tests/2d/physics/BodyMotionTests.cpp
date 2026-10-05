#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

class BodyMotionTest : public ::testing::Test {
  protected:
    void simulate(float seconds) {
        for (int step = 0; step < static_cast<int>(seconds * 60.0F); ++step) {
            world.step(1.0F / 60.0F);
        }
    }

    Body floor(float y) {
        Body body = world.createBody({.type = Body::Type::Static, .position = {0.0F, y}});
        body.addBox({4000.0F, 20.0F});
        return body;
    }

    World world;
};

TEST_F(BodyMotionTest, KinematicPlatformsMovedToTargetsCarryBodies) {
    // A platform driven by targets moves by velocity, so a crate on it rides along instead of falling through.
    Body platform = world.createBody({.type = Body::Type::Kinematic, .position = {0.0F, 0.0F}});
    platform.addBox({200.0F, 20.0F});
    Body crate = world.createBody({.position = {0.0F, -26.0F}});
    crate.addBox({32.0F, 32.0F});
    simulate(0.5F);
    for (int step = 1; step <= 60; ++step) {
        platform.moveTo({static_cast<float>(step) * 2.0F, -static_cast<float>(step) * 3.0F}, 0.0F, 1.0F / 60.0F);
        EXPECT_NEAR(platform.getVelocity().y, -180.0F, 0.1F);
        world.step(1.0F / 60.0F);
    }
    EXPECT_NEAR(platform.getPosition().y, -180.0F, 0.1F);
    EXPECT_NEAR(crate.getPosition().y - platform.getPosition().y, -26.0F, 1.0F);
    EXPECT_NEAR(crate.getPosition().x - platform.getPosition().x, 0.0F, 2.0F);

    // A target the platform already holds stops it, and a move slower than the sleep threshold still moves it.
    const math::Vec2 stop = platform.getPosition();
    platform.moveTo(stop, 0.0F, 1.0F / 60.0F);
    world.step(1.0F / 60.0F);
    EXPECT_NEAR(math::Vec2::distance(platform.getPosition(), stop), 0.0F, 1e-3F);
    platform.moveTo(stop + math::Vec2{0.5F, 0.0F}, 0.0F, 1.0F);
    EXPECT_NEAR(platform.getVelocity().x, 0.5F, 1e-3F);
    EXPECT_THROW(platform.moveTo({}, 0.0F, 0.0F), std::invalid_argument);
}

TEST_F(BodyMotionTest, ReportsTheVelocityOfWorldPoints) {
    World space({.gravity = {}});
    Body wheel = space.createBody({.position = {0.0F, 0.0F}, .velocity = {10.0F, 0.0F}, .angularVelocity = 2.0F});
    wheel.addCircle(50.0F);
    const math::Vec2 rim = wheel.getVelocityAt({0.0F, 50.0F});
    EXPECT_NEAR(rim.x, 10.0F - 100.0F, 0.01F);
    EXPECT_NEAR(rim.y, 0.0F, 0.01F);
}

TEST_F(BodyMotionTest, LowCentersOfMassTurnBodiesUpright) {
    // A box whose center of mass sits under its bottom rights itself from a 60 degree tilt, like a keel.
    floor(200.0F);
    Body tumbler = world.createBody({.position = {0.0F, 150.0F}, .rotation = std::numbers::pi_v<float> / 3.0F});
    tumbler.addBox({60.0F, 60.0F});
    Body::MassData data = tumbler.getMassData();
    EXPECT_NEAR(data.center.y, 0.0F, 1e-3F);
    tumbler.setCenterOfMass({0.0F, 40.0F});
    EXPECT_NEAR(tumbler.getCenterOfMass().y, 40.0F, 1e-3F);
    EXPECT_NEAR(tumbler.getMass(), data.mass, 1e-4F);
    simulate(3.0F);
    EXPECT_NEAR(std::remainder(tumbler.getRotation(), std::numbers::pi_v<float> / 2.0F), 0.0F, 0.05F);

    tumbler.setMass(data.mass * 2.0F);
    EXPECT_NEAR(tumbler.getInertia(), data.inertia * 2.0F, data.inertia * 0.01F);
    tumbler.resetMassData();
    EXPECT_NEAR(tumbler.getMass(), data.mass, 1e-4F);
    EXPECT_NEAR(tumbler.getCenterOfMass().y, 0.0F, 1e-3F);
    EXPECT_THROW(tumbler.setMass(-1.0F), std::invalid_argument);
}

TEST_F(BodyMotionTest, RollingResistanceStopsBalls) {
    floor(100.0F);
    Body rolling = world.createBody({.position = {-500.0F, 60.0F}, .velocity = {300.0F, 0.0F}});
    rolling.addCircle(20.0F);
    Body braked = world.createBody({.position = {500.0F, 60.0F}, .velocity = {300.0F, 0.0F}});
    Shape tire = braked.addCircle(20.0F, {.rollingResistance = 0.3F});
    EXPECT_FLOAT_EQ(tire.getRollingResistance(), 0.3F);
    simulate(4.0F);
    EXPECT_GT(std::abs(rolling.getVelocity().x), 100.0F);
    EXPECT_LT(std::abs(braked.getVelocity().x), 5.0F);
}

TEST_F(BodyMotionTest, FastRotationLetsWheelsSpinPastAnEighthOfATurnPerStep) {
    World space({.gravity = {}});
    Body slow = space.createBody({.angularVelocity = 100.0F});
    slow.addCircle(10.0F);
    Body fast = space.createBody({.angularVelocity = 100.0F, .fastRotation = true});
    fast.addCircle(10.0F);
    space.step(1.0F / 60.0F);
    EXPECT_LT(slow.getAngularVelocity(), 50.0F);
    EXPECT_NEAR(fast.getAngularVelocity(), 100.0F, 0.1F);
}

TEST_F(BodyMotionTest, ChangesMaterialsAndSleepWhileRunning) {
    // A box on a 30 degree ramp slides without friction and stays once its friction rises.
    Body ramp = world.createBody({.type = Body::Type::Static, .rotation = std::numbers::pi_v<float> / 6.0F});
    ramp.addBox({1000.0F, 20.0F});
    const math::Vec2 start{15.25F, -26.41F};
    Body box = world.createBody({.position = start, .rotation = std::numbers::pi_v<float> / 6.0F});
    Shape skin = box.addBox({40.0F, 40.0F}, {.friction = 0.0F});
    simulate(0.5F);
    EXPECT_GT(math::Vec2::distance(start, box.getPosition()), 20.0F);

    box.setTransform(start, std::numbers::pi_v<float> / 6.0F);
    box.setVelocity({});
    box.setAngularVelocity(0.0F);
    skin.setFriction(1.0F);
    skin.setRestitution(0.2F);
    skin.setDensity(3.0F);
    EXPECT_FLOAT_EQ(skin.getFriction(), 1.0F);
    EXPECT_FLOAT_EQ(skin.getRestitution(), 0.2F);
    EXPECT_FLOAT_EQ(skin.getDensity(), 3.0F);
    EXPECT_NEAR(box.getMass(), 3.0F * 40.0F * 40.0F / (64.0F * 64.0F), 1e-3F);
    simulate(1.0F);
    EXPECT_LT(math::Vec2::distance(start, box.getPosition()), 2.0F);
    EXPECT_THROW(skin.setFriction(-1.0F), std::invalid_argument);

    box.setSleepThreshold(10.0F);
    EXPECT_NEAR(box.getSleepThreshold(), 10.0F, 1e-3F);
    box.setSleepEnabled(false);
    EXPECT_FALSE(box.isSleepEnabled());
}

TEST_F(BodyMotionTest, ListsItsContactsAndTheShapesInsideSensors) {
    floor(100.0F);
    Body crate = world.createBody({.position = {0.0F, 70.0F}});
    crate.addBox({40.0F, 40.0F});
    Body zone = world.createBody({.type = Body::Type::Static, .position = {0.0F, 50.0F}});
    Shape sensor = zone.addBox({200.0F, 40.0F}, {.sensor = true});
    simulate(1.0F);
    const std::vector<Body::Contact> contacts = crate.getContacts();
    ASSERT_EQ(contacts.size(), 1U);
    EXPECT_NEAR(contacts[0].normal.y, 1.0F, 1e-3F);
    EXPECT_GT(contacts[0].impulse, 0.0F);
    EXPECT_EQ(sensor.getOverlaps().size(), 1U);
    EXPECT_THROW((void)contacts[0].other.getOverlaps(), std::logic_error);
}

TEST_F(BodyMotionTest, OpenChainsCollideAlongEveryListedSegment) {
    // A ball thrown at the last segment of an open chain, which Box2D would leave without collision, bounces back.
    World space({.gravity = {}});
    Body walls = space.createBody({.type = Body::Type::Static});
    walls.addChain(std::vector<math::Vec2>{{300.0F, 100.0F}, {100.0F, 100.0F}, {100.0F, -100.0F}}, false);
    Body ball = space.createBody({.velocity = {400.0F, 0.0F}});
    ball.addCircle(8.0F, {.restitution = 1.0F});
    for (int step = 0; step < 60; ++step) {
        space.step(1.0F / 60.0F);
    }
    EXPECT_LT(ball.getPosition().x, 100.0F);
    EXPECT_EQ(walls.getShapes().size(), 2U);
    EXPECT_EQ(walls.getOutlines().front().points.size(), 3U);
    EXPECT_THROW((void)walls.addChain(std::vector<math::Vec2>{{0.0F, 0.0F}}, false), std::invalid_argument);

    // Destroying a segment destroys the whole chain.
    walls.getShapes().front().destroy();
    EXPECT_TRUE(walls.getShapes().empty());
}

} // namespace haylen::physics2d
