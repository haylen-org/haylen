#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "haylen/2d/physics/Fluid.hpp"
#include "haylen/2d/physics/Ragdoll.hpp"
#include "haylen/2d/physics/Rope.hpp"
#include "haylen/2d/physics/Vehicle.hpp"
#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

class AssemblyTest : public ::testing::Test {
  protected:
    void simulate(float seconds) {
        for (int step = 0; step < static_cast<int>(seconds * 60.0F); ++step) {
            world.step(1.0F / 60.0F);
        }
    }

    Body ground(math::Vec2 center, math::Vec2 size) {
        Body body = world.createBody({.type = Body::Type::Static, .position = center});
        body.addBox(size);
        return body;
    }

    World world;
};

TEST_F(AssemblyTest, RopesChainTheirSegmentsAndHang) {
    Body hook = world.createBody({.type = Body::Type::Static, .position = {0.0F, 0.0F}});
    Rope rope = Rope::create(world, {.start = {0.0F, 0.0F}, .end = {200.0F, 0.0F}, .segments = 10, .linearDamping = 1.0F, .startBody = hook});
    EXPECT_EQ(rope.getBodies().size(), 10U);
    EXPECT_EQ(rope.getJoints().size(), 10U);
    EXPECT_FLOAT_EQ(rope.getSegmentLength(), 20.0F);
    EXPECT_EQ(rope.getPoints().size(), 11U);
    EXPECT_NEAR(rope.getPoints().back().x, 200.0F, 1e-3F);

    // The free end swings down below the hook while the first point stays on it.
    simulate(6.0F);
    const std::vector<math::Vec2> points = rope.getPoints();
    EXPECT_NEAR(math::Vec2::distance(points.front(), {0.0F, 0.0F}), 0.0F, 1.5F);
    EXPECT_GT(points.back().y, 150.0F);
    EXPECT_EQ(rope.getSegments().size(), 10U);
    EXPECT_FLOAT_EQ(rope.getSegments().front().length, 20.0F);

    rope.destroy();
    EXPECT_FALSE(rope.isValid());
    EXPECT_EQ(world.getBodyCount(), 1U);
    EXPECT_THROW((void)Rope::create(world, {.segments = 0}), std::invalid_argument);
}

TEST_F(AssemblyTest, BridgesHoldLoadsBetweenPinnedEnds) {
    Rope bridge = Rope::createBridge(world, {.start = {0.0F, 100.0F}, .end = {300.0F, 100.0F}, .segments = 12, .thickness = 8.0F});
    // Eleven joints between planks, one at each end, and two anchors of their own.
    EXPECT_EQ(bridge.getJoints().size(), 13U);
    EXPECT_EQ(world.getBodyCount(), 14U);

    Body crate = world.createBody({.position = {150.0F, 60.0F}});
    crate.addBox({30.0F, 30.0F});
    simulate(3.0F);
    // The bridge sags under the crate without letting it fall through.
    EXPECT_GT(crate.getPosition().y, 60.0F);
    EXPECT_LT(crate.getPosition().y, 200.0F);

    bridge.destroy();
    EXPECT_EQ(world.getBodyCount(), 1U);
}

TEST_F(AssemblyTest, RagdollsFallAsOneFigure) {
    ground({0.0F, 300.0F}, {1000.0F, 20.0F});
    Ragdoll ragdoll = Ragdoll::create(world, {.position = {0.0F, 100.0F}, .height = 128.0F, .jointFriction = 10.0F});
    EXPECT_EQ(ragdoll.getBodies().size(), Ragdoll::kPartCount);
    EXPECT_EQ(ragdoll.getJoints().size(), Ragdoll::kPartCount - 1);
    EXPECT_LT(ragdoll.getBody(Ragdoll::Part::Head).getPosition().y, ragdoll.getBody(Ragdoll::Part::LowerLegLeft).getPosition().y);

    simulate(4.0F);
    // Everything rests on the ground, and the joints keep the head near the chest.
    for (const Body& part : ragdoll.getBodies()) {
        EXPECT_LT(part.getPosition().y, 295.0F);
        EXPECT_GT(part.getPosition().y, 150.0F);
    }
    EXPECT_LT(math::Vec2::distance(ragdoll.getBody(Ragdoll::Part::Head).getPosition(), ragdoll.getBody(Ragdoll::Part::Chest).getPosition()), 64.0F);

    EXPECT_EQ(Ragdoll::partFromName("lowerLegRight"), Ragdoll::Part::LowerLegRight);
    EXPECT_EQ(Ragdoll::partName(Ragdoll::Part::Chest), "chest");
    EXPECT_FALSE(Ragdoll::partFromName("tail").has_value());
    ragdoll.destroy();
    EXPECT_FALSE(ragdoll.isValid());
    EXPECT_THROW((void)Ragdoll::create(world, {.group = 1}), std::invalid_argument);
}

TEST_F(AssemblyTest, VehiclesDriveOnTheirMotors) {
    ground({0.0F, 200.0F}, {4000.0F, 20.0F});
    Vehicle car = Vehicle::create(world, {.position = {0.0F, 140.0F}, .drive = Vehicle::Drive::All});
    EXPECT_TRUE(car.isValid());
    simulate(1.0F);
    const float start = car.getChassis().getPosition().x;

    car.setMotorSpeed(10.0F);
    EXPECT_FLOAT_EQ(car.getMotorSpeed(), 10.0F);
    simulate(2.0F);
    EXPECT_GT(car.getChassis().getPosition().x, start + 100.0F);
    // The wheels hang below the chassis on their springs.
    EXPECT_GT(car.getRearWheel().getPosition().y, car.getChassis().getPosition().y);
    EXPECT_GT(car.getFrontWheel().getPosition().x, car.getRearWheel().getPosition().x);

    car.setMotorSpeed(-10.0F);
    simulate(3.0F);
    EXPECT_LT(car.getChassis().getPosition().x, start + 100.0F);

    EXPECT_EQ(Vehicle::driveFromName("front"), Vehicle::Drive::Front);
    EXPECT_EQ(Vehicle::driveName(Vehicle::Drive::All), "all");
    car.destroy();
    EXPECT_FALSE(car.isValid());
    EXPECT_THROW((void)Vehicle::create(world, {.wheelRadius = 0.0F}), std::invalid_argument);
}

TEST_F(AssemblyTest, OneWayPlatformsLetBodiesUpFromBelow) {
    Body platform = world.createBody({.type = Body::Type::Static, .position = {0.0F, 0.0F}});
    Shape surface = platform.addBox({200.0F, 10.0F}, {.oneWay = math::Vec2{0.0F, -1.0F}});
    EXPECT_EQ(surface.getOneWay(), math::Vec2(0.0F, -1.0F));

    // A ball thrown up from below passes through, then lands on top.
    Body ball = world.createBody({.position = {0.0F, 60.0F}, .velocity = {0.0F, -700.0F}});
    ball.addCircle(8.0F);
    simulate(2.5F);
    EXPECT_LT(ball.getPosition().y, -5.0F);
    EXPECT_NEAR(ball.getVelocity().y, 0.0F, 1.0F);

    // A solid platform stops the same throw from below.
    surface.setOneWay(std::nullopt);
    EXPECT_FALSE(surface.getOneWay().has_value());
    Body blocked = world.createBody({.position = {60.0F, 60.0F}, .velocity = {0.0F, -700.0F}});
    blocked.addCircle(8.0F);
    simulate(2.0F);
    EXPECT_GT(blocked.getPosition().y, 5.0F);
    EXPECT_THROW(surface.setOneWay(math::Vec2{}), std::invalid_argument);
}

TEST_F(AssemblyTest, OneWayDirectionsTurnWithTheirBody) {
    // Upside down, a platform that points up lets bodies fall through from above and stops them from below.
    Body platform = world.createBody({.type = Body::Type::Static, .position = {0.0F, 0.0F}, .rotation = std::numbers::pi_v<float>});
    platform.addBox({200.0F, 10.0F}, {.oneWay = math::Vec2{0.0F, -1.0F}});
    Body falling = world.createBody({.position = {-50.0F, -60.0F}});
    falling.addCircle(8.0F);
    Body thrown = world.createBody({.position = {50.0F, 60.0F}, .velocity = {0.0F, -700.0F}});
    thrown.addCircle(8.0F);
    simulate(1.0F);
    EXPECT_GT(falling.getPosition().y, 20.0F);
    EXPECT_GT(thrown.getPosition().y, 5.0F);
}

TEST_F(AssemblyTest, ConveyorsCarryBodiesAlongTheirSurface) {
    Body belt = world.createBody({.type = Body::Type::Static, .position = {0.0F, 0.0F}});
    Shape surface = belt.addBox({2000.0F, 10.0F}, {.friction = 0.8F, .tangentSpeed = 120.0F});
    EXPECT_NEAR(surface.getTangentSpeed(), 120.0F, 1e-3F);

    Body crate = world.createBody({.position = {0.0F, -20.0F}});
    crate.addBox({20.0F, 20.0F});
    simulate(2.0F);
    EXPECT_GT(crate.getPosition().x, 100.0F);
    EXPECT_NEAR(crate.getVelocity().x, 120.0F, 10.0F);

    surface.setTangentSpeed(-120.0F);
    simulate(2.0F);
    EXPECT_LT(crate.getVelocity().x, -100.0F);
}

TEST_F(AssemblyTest, FluidsSettleInsideTheirContainer) {
    ground({0.0F, 200.0F}, {400.0F, 20.0F});
    ground({-200.0F, 100.0F}, {20.0F, 220.0F});
    ground({200.0F, 100.0F}, {20.0F, 220.0F});

    Fluid fluid(world, {.radius = 4.0F, .smoothingRadius = 16.0F, .maxParticles = 300});
    EXPECT_EQ(fluid.fill({-120.0F, 0.0F, 240.0F, 80.0F}), 300U);
    EXPECT_FALSE(fluid.spawn({0.0F, 0.0F}));
    EXPECT_EQ(fluid.size(), 300U);

    for (int step = 0; step < 240; ++step) {
        fluid.update(1.0F / 60.0F);
        world.step(1.0F / 60.0F);
    }

    // The liquid spreads over the floor between the walls and comes to rest.
    std::vector<float> positions(fluid.size() * 2);
    EXPECT_EQ(fluid.copyPositions(positions), 300U);
    std::vector<float> velocities(fluid.size() * 2);
    EXPECT_EQ(fluid.copyVelocities(velocities), 300U);
    float lowest = 1e9F;
    float highest = -1e9F;
    for (std::size_t index = 0; index < fluid.size(); ++index) {
        const float x = positions[index * 2];
        const float y = positions[index * 2 + 1];
        EXPECT_GT(x, -190.0F);
        EXPECT_LT(x, 190.0F);
        EXPECT_LT(y, 190.0F);
        EXPECT_LT(std::hypot(velocities[index * 2], velocities[index * 2 + 1]), 200.0F);
        lowest = std::min(lowest, x);
        highest = std::max(highest, x);
    }
    EXPECT_GT(highest - lowest, 250.0F);

    fluid.remove(0);
    EXPECT_EQ(fluid.size(), 299U);
    EXPECT_THROW(fluid.remove(500), std::out_of_range);
    fluid.clear();
    EXPECT_EQ(world.getBodyCount(), 3U);
    EXPECT_THROW(Fluid(world, {.radius = 4.0F, .smoothingRadius = 2.0F}), std::invalid_argument);
}

} // namespace haylen::physics2d
