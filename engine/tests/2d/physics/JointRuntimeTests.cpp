#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

class JointRuntimeTest : public ::testing::Test {
  protected:
    void simulate(float seconds) {
        for (int step = 0; step < static_cast<int>(seconds * 60.0F); ++step) {
            world.step(1.0F / 60.0F);
        }
    }

    Body box(math::Vec2 position, float density = 1.0F) {
        Body body = world.createBody({.position = position});
        body.addBox({20.0F, 20.0F}, {.density = density});
        return body;
    }

    World world;
    Body anchor = world.createBody({.type = Body::Type::Static});
};

TEST_F(JointRuntimeTest, RevoluteJointsReportTheirAngleAndChangeWhileRunning) {
    World space({.gravity = {}});
    const Body pin = space.createBody({.type = Body::Type::Static});
    Body lever = space.createBody({.position = {50.0F, 0.0F}});
    lever.addBox({100.0F, 10.0F});
    Joint hinge = space.createJoint(Joint::Type::Revolute, pin, lever, {.enableSpring = true, .hertz = 4.0F, .dampingRatio = 0.7F});
    EXPECT_EQ(hinge.getType(), Joint::Type::Revolute);
    EXPECT_EQ(hinge.getBodyA(), pin);
    EXPECT_EQ(hinge.getBodyB(), lever);
    // clang-format off
    const auto run = [&space](float seconds) {
        for (int step = 0; step < static_cast<int>(seconds * 60.0F); ++step) {
            space.step(1.0F / 60.0F);
        }
    };
    // clang-format on
    run(2.0F);
    EXPECT_NEAR(hinge.getAngle(), 0.0F, 0.05F);

    // The spring pulls the lever to a new target, and limits stop it short of the target.
    hinge.setTargetAngle(0.8F);
    EXPECT_FLOAT_EQ(hinge.getTargetAngle(), 0.8F);
    run(3.0F);
    EXPECT_NEAR(hinge.getAngle(), 0.8F, 0.05F);
    EXPECT_NEAR(hinge.getAngle(), lever.getRotation(), 1e-3F);
    hinge.setLimits(-0.2F, 0.3F);
    hinge.setLimitEnabled(true);
    EXPECT_TRUE(hinge.isLimitEnabled());
    EXPECT_FLOAT_EQ(hinge.getLower(), -0.2F);
    EXPECT_FLOAT_EQ(hinge.getUpper(), 0.3F);
    run(2.0F);
    EXPECT_LT(hinge.getAngle(), 0.32F);

    hinge.setHertz(2.0F);
    hinge.setDampingRatio(0.2F);
    EXPECT_FLOAT_EQ(hinge.getHertz(), 2.0F);
    EXPECT_FLOAT_EQ(hinge.getDampingRatio(), 0.2F);
    hinge.setMotorEnabled(true);
    hinge.setMaxMotorTorque(1000.0F);
    EXPECT_NEAR(hinge.getMaxMotorTorque(), 1000.0F, 0.1F);
    EXPECT_THROW(hinge.setLimits(1.0F, 0.0F), std::invalid_argument);
    EXPECT_THROW(hinge.setLimits(-4.0F, 0.0F), std::invalid_argument);
    EXPECT_THROW((void)hinge.getLength(), std::logic_error);
    EXPECT_THROW((void)hinge.getTarget(), std::logic_error);
}

TEST_F(JointRuntimeTest, RopeJointsSlackAndLimitTheirLength) {
    // A distance joint with a spring of no stiffness is slack below its maximum, which reeling in shortens.
    Body hero = box({0.0F, 100.0F});
    Joint rope = world.createJoint(Joint::Type::Distance, anchor, hero, {.anchorB = {0.0F, 100.0F}, .enableLimit = true, .lower = 0.0F, .upper = 200.0F, .enableSpring = true});
    float farthest = 0.0F;
    for (int step = 0; step < 300; ++step) {
        world.step(1.0F / 60.0F);
        farthest = std::max(farthest, hero.getPosition().getLength());
    }
    EXPECT_NEAR(rope.getCurrentLength(), 200.0F, 2.0F);
    EXPECT_LT(farthest, 203.0F);
    EXPECT_NEAR(math::Vec2::distance(rope.getAnchorA(), {0.0F, 0.0F}), 0.0F, 0.01F);
    EXPECT_NEAR(math::Vec2::distance(rope.getAnchorB(), hero.getPosition()), 0.0F, 0.01F);

    rope.setLimits(0.0F, 120.0F);
    simulate(2.0F);
    EXPECT_LT(rope.getCurrentLength(), 122.0F);
    rope.setLength(80.0F);
    EXPECT_NEAR(rope.getLength(), 80.0F, 1e-3F);
    EXPECT_THROW(rope.setLength(0.0F), std::invalid_argument);
}

TEST_F(JointRuntimeTest, PrismaticLiftsCarryLoadsToTheirLimit) {
    // A lift whose motor force is twice the weight of itself and its load raises the load to the upper limit.
    Body lift = box({0.0F, 0.0F});
    Joint piston = world.createJoint(Joint::Type::Prismatic, anchor, lift, {.enableLimit = true, .lower = -300.0F, .upper = 0.0F, .enableMotor = true, .motorSpeed = -200.0F, .axis = {0.0F, 1.0F}});
    Body crate = box({0.0F, -21.0F});
    const float weight = (lift.getMass() + crate.getMass()) * 980.0F;
    piston.setMaxMotorForce(weight * 2.0F);
    EXPECT_NEAR(piston.getMaxMotorForce(), weight * 2.0F, weight * 0.01F);
    simulate(3.0F);
    EXPECT_NEAR(piston.getTranslation(), -300.0F, 1.0F);
    EXPECT_LT(crate.getPosition().y, -310.0F);
    piston.setTargetTranslation(-100.0F);
    EXPECT_NEAR(piston.getTargetTranslation(), -100.0F, 1e-3F);
}

TEST_F(JointRuntimeTest, JointsBreakAboveTheirBreakForce) {
    // A weld holds a box under a break force of five times its weight, and lets go once the load passes it.
    Body shelf = box({0.0F, 0.0F});
    const float weight = shelf.getMass() * 980.0F;
    Joint weld = world.createJoint(Joint::Type::Weld, anchor, shelf, {.breakForce = weight * 5.0F});
    EXPECT_NEAR(weld.getBreakForce().value_or(0.0F), weight * 5.0F, weight * 0.01F);
    simulate(2.0F);
    EXPECT_TRUE(weld.isValid());
    EXPECT_NEAR(weld.getConstraintForce().getLength(), weight, weight * 0.05F);

    Body anvil = box({0.0F, -100.0F}, 10.0F);
    std::size_t breaks = 0;
    for (int step = 0; step < 120 && breaks == 0; ++step) {
        world.step(1.0F / 60.0F);
        breaks += world.getJointBreaks().size();
    }
    EXPECT_EQ(breaks, 1U);
    EXPECT_FALSE(weld.isValid());
    EXPECT_GT(anvil.getMass(), 0.0F);
    EXPECT_THROW((void)world.createJoint(Joint::Type::Weld, anchor, anvil, {.breakTorque = -1.0F}), std::invalid_argument);
}

TEST_F(JointRuntimeTest, MotorAndMouseJointsChangeWhileRunning) {
    Body puppet = box({100.0F, 0.0F});
    Joint motor = world.createJoint(Joint::Type::Motor, anchor, puppet, {.maxMotorForce = 100000.0F, .maxMotorTorque = 100000.0F});
    EXPECT_NEAR(motor.getLinearOffset().x, 100.0F, 1e-3F);
    motor.setLinearOffset({0.0F, -100.0F});
    motor.setAngularOffset(0.5F);
    simulate(2.0F);
    EXPECT_NEAR(puppet.getPosition().y, -100.0F, 2.0F);
    EXPECT_NEAR(puppet.getRotation(), 0.5F, 0.05F);

    Body crate = box({400.0F, 0.0F});
    Joint drag = world.createJoint(Joint::Type::Mouse, anchor, crate, {.anchorB = {400.0F, 0.0F}});
    drag.setMaxMotorForce(crate.getMass() * 40000.0F);
    drag.setHertz(6.0F);
    drag.setTarget({400.0F, -200.0F});
    simulate(1.0F);
    EXPECT_NEAR(crate.getPosition().y, -200.0F, 5.0F);
    drag.setConstraintHertz(30.0F);
    drag.setConstraintDampingRatio(1.0F);
    EXPECT_FLOAT_EQ(drag.getConstraintHertz(), 30.0F);
    EXPECT_FLOAT_EQ(drag.getConstraintDampingRatio(), 1.0F);
    EXPECT_THROW(drag.setConstraintHertz(0.0F), std::invalid_argument);
}

} // namespace haylen::physics2d
