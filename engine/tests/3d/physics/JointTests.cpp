#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "3d/physics/jolt/Backend.hpp"
#include "haylen/3d/physics/Body.hpp"
#include "haylen/3d/physics/Joint.hpp"
#include "haylen/3d/physics/Shape.hpp"
#include "haylen/3d/physics/World.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Spline3D.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::physics3d {

class Joint3DTest : public ::testing::Test {
  protected:
    static constexpr float kStep = 1.0F / 60.0F;

    // The solver holds limits with soft contacts, so a body pushed hard against a limit passes it by a little.
    static constexpr float kLimitSlack = 0.05F;

    [[nodiscard]] std::shared_ptr<const Shape> make(Shape::Geometry geometry) const {
        return backend.createShape({.geometry = std::move(geometry)});
    }

    static void run(World& world, int steps) {
        for (int step = 0; step < steps; ++step) {
            world.step(kStep);
        }
    }

    // Returns the message of the exception of the type that the call throws, or an empty text when it throws none.
    template <typename Error, typename Call> static std::string refusal(Call call) {
        try {
            call();
        } catch (const Error& error) {
            return error.what();
        }

        return {};
    }

    jolt::Backend backend;
    std::shared_ptr<const Shape> crate = make(Shape::Box{.size = {1.0F, 1.0F, 1.0F}});
    std::shared_ptr<const Shape> plank = make(Shape::Box{.size = {2.0F, 0.2F, 0.2F}});
};

TEST_F(Joint3DTest, KeepsAHingeWithinItsLimitsUnderAStrongTorque) {
    World world({.gravity = {}}, backend);
    Body door = world.createBody({.shape = plank, .position = {1.0F, 0.0F, 0.0F}, .angularDamping = 0.0F});
    const Joint hinge = world.createJoint(Joint::Type::Hinge, door, std::nullopt, {.point = {}, .axis = {0.0F, 1.0F, 0.0F}, .normal = {1.0F, 0.0F, 0.0F}, .lower = -0.5F, .upper = 0.5F});
    EXPECT_EQ(hinge.getType(), Joint::Type::Hinge);
    EXPECT_FALSE(hinge.getSecond().has_value());
    for (int step = 0; step < 120; ++step) {
        door.applyTorque({0.0F, 1000.0F, 0.0F});
        world.step(kStep);
        ASSERT_LE(std::abs(hinge.getAngle()), 0.5F + kLimitSlack);
    }

    EXPECT_NEAR(std::abs(hinge.getAngle()), 0.5F, kLimitSlack);
    EXPECT_NEAR(math::Vec3::distance(door.getWorldCenter(), {}), 1.0F, 0.01F);
    EXPECT_GT(hinge.getLoad().torque, 0.0F);
}

TEST_F(Joint3DTest, KeepsABallJointInsideItsConeUnderStress) {
    World world({}, backend);
    Body weight = world.createBody({.shape = crate, .position = {0.0F, -2.0F, 0.0F}});
    const Joint ball = world.createJoint(Joint::Type::SwingTwist, weight, std::nullopt, {.point = {0.0F, 0.0F, 0.0F}, .axis = {0.0F, -1.0F, 0.0F}, .normal = {1.0F, 0.0F, 0.0F}, .halfConeAngle = 0.3F, .planeHalfConeAngle = 0.3F});
    for (int step = 0; step < 180; ++step) {
        weight.applyForce({weight.getMass() * math::Math::kStandardGravity, 0.0F, weight.getMass() * 0.5F * math::Math::kStandardGravity});
        world.step(kStep);
        const math::Vec3 arm = weight.getWorldCenter().getNormalized();
        ASSERT_LE(std::acos(std::clamp(-arm.y, -1.0F, 1.0F)), 0.3F + kLimitSlack);
    }

    EXPECT_NEAR(weight.getWorldCenter().getLength(), 2.0F, 0.02F);
    EXPECT_NEAR(ball.getHalfConeAngle(), 0.3F, 1.0e-5F);
}

TEST_F(Joint3DTest, BreaksAJointOnceWhenItsLoadPassesTheLimit) {
    World world({.sleepEnabled = false}, backend);
    Body weight = world.createBody({.shape = crate, .position = {0.0F, -1.0F, 0.0F}});
    const Joint joint = world.createJoint(Joint::Type::Fixed, weight, std::nullopt, {.point = {0.0F, -0.5F, 0.0F}, .breakForce = 2.0F * weight.getMass() * math::Math::kStandardGravity});
    run(world, 30);
    EXPECT_TRUE(joint.isValid());
    EXPECT_TRUE(world.getJointBreaks().empty());

    weight.applyForce({0.0F, -4.0F * weight.getMass() * math::Math::kStandardGravity, 0.0F});
    world.step(kStep);
    ASSERT_EQ(world.getJointBreaks().size(), 1U);
    const World::JointBreak& broken = world.getJointBreaks().front();
    EXPECT_EQ(broken.joint, joint);
    EXPECT_EQ(broken.first, weight);
    EXPECT_FALSE(broken.second.has_value());
    EXPECT_GT(broken.load.force, 2.0F * weight.getMass() * math::Math::kStandardGravity);
    EXPECT_FALSE(joint.isValid());

    world.step(kStep);
    EXPECT_TRUE(world.getJointBreaks().empty());
    run(world, 30);
    EXPECT_LT(weight.getPosition().y, -2.0F);
}

TEST_F(Joint3DTest, DrivesHingesAndSlidersWithTheirMotors) {
    World world({.gravity = {}}, backend);
    Body wheel = world.createBody({.shape = make(Shape::Cylinder{.height = 0.2F, .radius = 0.5F}), .angularDamping = 0.0F});
    Joint axle = world.createJoint(Joint::Type::Hinge, wheel, std::nullopt, {.axis = {0.0F, 1.0F, 0.0F}, .normal = {1.0F, 0.0F, 0.0F}, .motor = {.state = Joint::MotorState::Velocity, .targetVelocity = 3.0F}});
    Body carriage = world.createBody({.shape = crate, .position = {0.0F, 3.0F, 0.0F}});
    Joint rail = world.createJoint(Joint::Type::Slider, carriage, std::nullopt, {.point = {0.0F, 3.0F, 0.0F}, .axis = {1.0F, 0.0F, 0.0F}, .normal = {0.0F, 1.0F, 0.0F}, .lower = -2.0F, .upper = 2.0F});
    rail.setMotor({.state = Joint::MotorState::Position, .targetPosition = 1.5F, .spring = {.frequency = 4.0F, .damping = 1.0F}});
    run(world, 180);
    EXPECT_NEAR(wheel.getAngularVelocity().y, 3.0F, 0.05F);
    EXPECT_NEAR(rail.getPosition(), 1.5F, 0.02F);
    EXPECT_NEAR(carriage.getPosition().x, 1.5F, 0.02F);
    EXPECT_EQ(axle.getMotor().state, Joint::MotorState::Velocity);

    axle.setMotor({.state = Joint::MotorState::Off});
    rail.setLimits(-0.5F, 0.5F);
    run(world, 60);
    EXPECT_LE(carriage.getPosition().x, 0.5F + 0.02F);
    EXPECT_FLOAT_EQ(rail.getUpper(), 0.5F);
}

TEST_F(Joint3DTest, KeepsDistancesPointsAndPulleysAsTheyWereMade) {
    World world({}, backend);
    const Body anchor = world.createBody({.type = Body::Type::Static, .shape = crate, .position = {0.0F, 5.0F, 0.0F}});
    Body swinging = world.createBody({.shape = crate, .position = {2.0F, 5.0F, 0.0F}});
    const Joint rope = world.createJoint(Joint::Type::Distance, anchor, swinging, {.point = {0.0F, 5.0F, 0.0F}, .secondPoint = math::Vec3{2.0F, 5.0F, 0.0F}, .lower = 1.0F, .upper = 2.0F});
    Body pinned = world.createBody({.shape = crate, .position = {6.0F, 5.0F, 0.0F}});
    world.createJoint(Joint::Type::Point, pinned, std::nullopt, {.point = {6.5F, 5.5F, 0.0F}});
    Body left = world.createBody({.shape = crate, .position = {-4.0F, 2.0F, 0.0F}});
    Body right = world.createBody({.shape = crate, .position = {-2.0F, 2.0F, 0.0F}, .mass = 2000.0F});
    const Joint pulley = world.createJoint(Joint::Type::Pulley, left, right, {.point = {-4.0F, 2.5F, 0.0F}, .secondPoint = math::Vec3{-2.0F, 2.5F, 0.0F}, .fixedPoint = {-4.0F, 6.0F, 0.0F}, .secondFixedPoint = {-2.0F, 6.0F, 0.0F}});
    for (int step = 0; step < 120; ++step) {
        world.step(kStep);
        ASSERT_LE(rope.getLength(), 2.0F + 0.02F);
    }

    EXPECT_NEAR(math::Vec3::distance(pinned.getWorldCenter(), {6.5F, 5.5F, 0.0F}), std::sqrt(0.5F), 0.03F);
    EXPECT_GT(left.getPosition().y, 2.0F);
    EXPECT_LT(right.getPosition().y, 2.0F);
    EXPECT_NEAR((5.5F - left.getPosition().y) + (5.5F - right.getPosition().y), 7.0F, 0.05F);
    EXPECT_FLOAT_EQ(pulley.getRatio(), 1.0F);
    EXPECT_GE(rope.getLength(), 1.0F - 0.02F);
}

TEST_F(Joint3DTest, FreesLimitsAndFixesTheAxesOfASixDofJoint) {
    World world({}, backend);
    Body hanging = world.createBody({.shape = crate, .position = {0.0F, 0.0F, 0.0F}});
    Joint::Options options{.axis = {1.0F, 0.0F, 0.0F}, .normal = {0.0F, 1.0F, 0.0F}};
    for (Joint::AxisSettings& axis : options.axes) {
        axis.mode = Joint::AxisMode::Fixed;
    }

    options.axes[static_cast<std::size_t>(Joint::Axis::Y)] = {.mode = Joint::AxisMode::Limited, .lower = -1.0F, .upper = 0.0F};
    Joint six = world.createJoint(Joint::Type::SixDof, hanging, std::nullopt, options);
    run(world, 120);
    EXPECT_NEAR(hanging.getPosition().y, -1.0F, 0.03F);
    EXPECT_NEAR(hanging.getPosition().x, 0.0F, 1.0e-3F);

    six.setAxis(Joint::Axis::Y, {.mode = Joint::AxisMode::Free});
    run(world, 30);
    EXPECT_LT(hanging.getPosition().y, -2.0F);
    EXPECT_EQ(six.getAxis(Joint::Axis::Y).mode, Joint::AxisMode::Free);
    EXPECT_NEAR(hanging.getPosition().z, 0.0F, 1.0e-3F);
}

TEST_F(Joint3DTest, MovesABodyAlongItsPathAndGearsTwoHingesTogether) {
    World world({.gravity = {}}, backend);
    const Body track = world.createBody({.type = Body::Type::Static, .shape = crate, .position = {0.0F, -5.0F, 0.0F}});
    Body cart = world.createBody({.shape = crate, .position = {0.0F, 0.0F, 0.0F}});
    const math::Spline3D path({{0.0F, 0.0F, 0.0F}, {4.0F, 0.0F, 0.0F}, {8.0F, 2.0F, 0.0F}});
    Joint rail = world.createJoint(Joint::Type::Path, track, cart, {.motor = {.state = Joint::MotorState::Position, .targetPosition = 1.0F, .spring = {.frequency = 2.0F, .damping = 1.0F}}, .path = path});
    run(world, 240);
    EXPECT_NEAR(rail.getFraction(), 1.0F, 0.02F);
    EXPECT_NEAR(math::Vec3::distance(cart.getPosition(), {8.0F, 2.0F, 0.0F}), 0.0F, 0.2F);

    Body driver = world.createBody({.shape = make(Shape::Cylinder{.height = 0.2F, .radius = 0.5F}), .position = {20.0F, 0.0F, 0.0F}, .angularDamping = 0.0F});
    Body driven = world.createBody({.shape = make(Shape::Cylinder{.height = 0.2F, .radius = 1.0F}), .position = {22.0F, 0.0F, 0.0F}, .angularDamping = 0.0F});
    const Joint first = world.createJoint(Joint::Type::Hinge, driver, std::nullopt, {.point = {20.0F, 0.0F, 0.0F}, .motor = {.state = Joint::MotorState::Velocity, .targetVelocity = 4.0F}});
    const Joint second = world.createJoint(Joint::Type::Hinge, driven, std::nullopt, {.point = {22.0F, 0.0F, 0.0F}});
    const Joint gear = world.createJoint(Joint::Type::Gear, driver, driven, {.ratio = 2.0F, .joints = {first, second}});
    run(world, 180);
    EXPECT_NEAR(std::abs(driven.getAngularVelocity().y) * 2.0F, std::abs(driver.getAngularVelocity().y), 0.1F);
    EXPECT_FLOAT_EQ(gear.getRatio(), 2.0F);
}

TEST_F(Joint3DTest, LetsTheBodiesOfAJointCollideOnlyWhenItSaysSo) {
    World world({.gravity = {}}, backend);
    Body first = world.createBody({.shape = crate});
    Body second = world.createBody({.shape = crate, .position = {0.5F, 0.0F, 0.0F}});
    world.createJoint(Joint::Type::Slider, first, second, {.axis = {1.0F, 0.0F, 0.0F}, .normal = {0.0F, 1.0F, 0.0F}});
    Body third = world.createBody({.shape = crate, .position = {0.0F, 10.0F, 0.0F}});
    Body fourth = world.createBody({.shape = crate, .position = {0.5F, 10.0F, 0.0F}});
    world.createJoint(Joint::Type::Slider, third, fourth, {.axis = {1.0F, 0.0F, 0.0F}, .normal = {0.0F, 1.0F, 0.0F}, .collideConnected = true});
    run(world, 60);
    EXPECT_NEAR(second.getPosition().x - first.getPosition().x, 0.5F, 0.02F);
    EXPECT_GT(fourth.getPosition().x - third.getPosition().x, 0.95F);
}

TEST_F(Joint3DTest, EndsTheJointsOfABodyWithItAndRefusesWhatItCannotHold) {
    World world({}, backend);
    Body first = world.createBody({.shape = crate});
    Body second = world.createBody({.shape = crate, .position = {2.0F, 0.0F, 0.0F}});
    const Joint joint = world.createJoint(Joint::Type::Hinge, first, second, {.point = {1.0F, 0.0F, 0.0F}});
    second.destroy();
    EXPECT_FALSE(joint.isValid());
    EXPECT_EQ(refusal<std::logic_error>([&] { static_cast<void>(joint.getAngle()); }), "The 3D physics joint was destroyed.");

    Body third = world.createBody({.shape = crate, .position = {4.0F, 0.0F, 0.0F}});
    const Joint glue = world.createJoint(Joint::Type::Fixed, first, third);
    EXPECT_EQ(refusal<std::logic_error>([&] { static_cast<void>(glue.getAngle()); }), "Only the joints \"hinge\" have an angle, and this joint is \"fixed\".");
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createJoint(Joint::Type::Fixed, first, first); }), "A 3D joint holds two different bodies, or one body and the world.");
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createJoint(Joint::Type::Hinge, first, third, {.lower = 1.0F, .upper = 0.0F}); }), "The limits of a 3D joint must be finite numbers, the lower one not above the upper one.");
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createJoint(Joint::Type::Hinge, first, third, {.axis = {0.0F, 1.0F, 0.0F}, .normal = {0.0F, 1.0F, 0.0F}}); }), "The normal of a 3D joint must stand at a right angle to its axis.");
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createJoint(Joint::Type::Gear, first, third, {.joints = {glue, glue}}); }), "A joint of the type \"gear\" takes as its joints two live hinges of its own world.");
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createJoint(Joint::Type::Path, first, third); }), "A path joint needs the option \"path\", a spline of two points or more.");
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createJoint(Joint::Type::Pulley, first, std::nullopt); }), "A joint of the type \"pulley\" needs a second body.");

    World other({}, backend);
    const Body stranger = other.createBody({.shape = crate});
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createJoint(Joint::Type::Fixed, first, stranger); }), "A 3D joint holds live bodies of its own world.");
}

TEST_F(Joint3DTest, StepsJointsWithBreakLimitsWithoutAllocating) {
    World world({.sleepEnabled = false}, backend);
    std::vector<Body> links;
    for (int index = 0; index < 10; ++index) {
        links.push_back(world.createBody({.shape = crate, .position = {static_cast<float>(index) * 1.1F, 0.0F, 0.0F}}));
        const std::optional<Body> previous = index > 0 ? std::optional<Body>(links[static_cast<std::size_t>(index) - 1]) : std::nullopt;
        world.createJoint(Joint::Type::Point, links.back(), previous, {.point = {static_cast<float>(index) * 1.1F - 0.55F, 0.0F, 0.0F}, .breakForce = 1.0e9F});
    }

    run(world, 2);
    const test::AllocationTracker tracker;
    run(world, 120);
    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_TRUE(world.getJointBreaks().empty());
}

} // namespace haylen::physics3d
