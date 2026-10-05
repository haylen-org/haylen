#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include "haylen/2d/physics/TopDownVehicle.hpp"
#include "haylen/2d/physics/Vehicle.hpp"
#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

class VehicleTest : public ::testing::Test {
  protected:
    void simulate(float seconds) {
        for (int step = 0; step < static_cast<int>(seconds * 60.0F); ++step) {
            world.step(1.0F / 60.0F);
        }
    }

    World world;
};

TEST_F(VehicleTest, DrivesOnItsMotorsAndBrakes) {
    Body road = world.createBody({.type = Body::Type::Static});
    road.addBox({20000.0F, 20.0F}, {.offset = {0.0F, 200.0F}});
    Vehicle car(world, {.position = {0.0F, 140.0F}, .drive = Vehicle::Drive::All});
    EXPECT_TRUE(car.isValid());
    simulate(1.0F);
    EXPECT_TRUE(car.isGrounded());
    const float start = car.getChassis().getPosition().x;

    car.setThrottle(2.0F);
    EXPECT_FLOAT_EQ(car.getThrottle(), 1.0F);
    simulate(2.0F);
    EXPECT_GT(car.getChassis().getPosition().x, start + 400.0F);
    EXPECT_GT(car.getSpeed(), 500.0F);
    // The wheels hang below the chassis on their springs.
    EXPECT_GT(car.getRearWheel().getPosition().y, car.getChassis().getPosition().y);

    car.setThrottle(0.0F);
    car.setBrake(1.0F);
    simulate(1.5F);
    EXPECT_NEAR(car.getSpeed(), 0.0F, 5.0F);

    EXPECT_EQ(Vehicle::driveFromName("front"), Vehicle::Drive::Front);
    EXPECT_EQ(Vehicle::driveName(Vehicle::Drive::All), "all");
    car.destroy();
    EXPECT_FALSE(car.isValid());
    EXPECT_THROW(Vehicle(world, {.wheelRadius = 0.0F}), std::invalid_argument);
    EXPECT_THROW(Vehicle(world, {.suspensionTravel = -1.0F}), std::invalid_argument);
    EXPECT_THROW(Vehicle(world, {.acceleration = 0.0F}), std::invalid_argument);
    EXPECT_THROW(Vehicle(world, {.antiRoll = 2.0F}), std::invalid_argument);
    EXPECT_THROW(Vehicle(world, {.wheelFriction = -1.0F}), std::invalid_argument);
    EXPECT_EQ(world.getBodyCount(), 1U);
}

TEST_F(VehicleTest, KeepsItsWheelsDownAndCoasts) {
    // Full throttle from rest and its release never stand the car on its wheels, and the released car rolls on.
    Body road = world.createBody({.type = Body::Type::Static});
    road.addBox({40000.0F, 20.0F}, {.offset = {0.0F, 200.0F}});
    Vehicle car(world, {.position = {0.0F, 140.0F}});
    simulate(0.5F);

    float steepest = 0.0F;
    car.setThrottle(1.0F);
    for (int step = 0; step < 180; ++step) {
        world.step(1.0F / 60.0F);
        steepest = std::max(steepest, std::abs(car.getChassis().getRotation()));
    }
    const float cruising = car.getSpeed();
    car.setThrottle(0.0F);
    for (int step = 0; step < 120; ++step) {
        world.step(1.0F / 60.0F);
        steepest = std::max(steepest, std::abs(car.getChassis().getRotation()));
    }
    EXPECT_LT(steepest, 0.35F);
    EXPECT_GT(car.getSpeed(), cruising * 0.5F);
    EXPECT_LT(car.getSpeed(), cruising);
}

TEST_F(VehicleTest, StopsAtTheWallsOfAnOpenRoad) {
    // The walls at both ends of an open chain collide, so a car at full throttle stays on its road.
    Body road = world.createBody({.type = Body::Type::Static});
    const std::vector<math::Vec2> outline{{-400.0F, -300.0F}, {-400.0F, 200.0F}, {1600.0F, 200.0F}, {1600.0F, -300.0F}};
    road.addChain(outline, false);
    Vehicle car(world, {.position = {0.0F, 140.0F}});
    car.setThrottle(1.0F);
    simulate(5.0F);
    EXPECT_LT(car.getChassis().getPosition().x, 1600.0F);
    car.setThrottle(-1.0F);
    simulate(6.0F);
    EXPECT_GT(car.getChassis().getPosition().x, -400.0F);
    EXPECT_LT(car.getChassis().getPosition().y, 200.0F);
}

TEST_F(VehicleTest, TurnsItsNoseInTheAir) {
    // Without ground under the wheels the throttle lifts the nose, which turns the chassis counter-clockwise on screen.
    World space({.gravity = {}});
    Vehicle car(space, {.position = {0.0F, 0.0F}, .airControl = 6.0F});
    car.setThrottle(1.0F);
    for (int step = 0; step < 30; ++step) {
        space.step(1.0F / 60.0F);
    }
    EXPECT_FALSE(car.isGrounded());
    EXPECT_LT(car.getChassis().getAngularVelocity(), -1.0F);
}

TEST_F(VehicleTest, TopDownCarsSteerGripAndDrift) {
    World flat({.gravity = {}});
    TopDownVehicle car(flat, {.position = {0.0F, 0.0F}});
    car.setThrottle(1.0F);
    for (int step = 0; step < 120; ++step) {
        flat.step(1.0F / 60.0F);
    }
    EXPECT_GT(car.getSpeed(), 600.0F);
    EXPECT_NEAR(car.getSlip(), 0.0F, 5.0F);

    // A gentle turn keeps the car on its tires, and the handbrake lets the rear slide out.
    car.setSteering(0.3F);
    for (int step = 0; step < 30; ++step) {
        flat.step(1.0F / 60.0F);
    }
    EXPECT_GT(car.getBody().getRotation(), 0.1F);
    EXPECT_GT(car.getSteeringAngle(), 0.0F);
    car.setSteering(1.0F);
    car.setHandbrake(true);
    bool drifted = false;
    for (int step = 0; step < 60; ++step) {
        flat.step(1.0F / 60.0F);
        drifted = drifted || car.isDrifting();
    }
    EXPECT_TRUE(drifted);

    car.setThrottle(0.0F);
    car.setHandbrake(false);
    car.setSteering(0.0F);
    car.setBrake(1.0F);
    for (int step = 0; step < 120; ++step) {
        flat.step(1.0F / 60.0F);
    }
    EXPECT_NEAR(car.getBody().getVelocity().getLength(), 0.0F, 5.0F);
    EXPECT_THROW(TopDownVehicle(flat, {.grip = 0.0F}), std::invalid_argument);
    EXPECT_THROW(TopDownVehicle(flat, {.frontAxle = -40.0F}), std::invalid_argument);
}

} // namespace haylen::physics2d
