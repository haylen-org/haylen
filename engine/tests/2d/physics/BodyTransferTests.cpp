#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

class BodyTransferTest : public ::testing::Test {
  protected:
    static void simulate(World& world, float seconds) {
        for (int step = 0; step < static_cast<int>(seconds * 60.0F); ++step) {
            world.step(1.0F / 60.0F);
        }
    }

    World ground{{.gravity = {0.0F, 980.0F}}};
    World space{{.gravity = {0.0F, 0.0F}}};
};

TEST_F(BodyTransferTest, AMovedBodyKeepsItsStateAndObeysTheWorldItLandsIn) {
    Body ball = ground.createBody({.position = {100.0F, -50.0F}, .rotation = 0.5F, .velocity = {300.0F, -120.0F}, .angularVelocity = 2.0F, .linearDamping = 0.25F, .angularDamping = 0.5F, .gravityScale = 1.5F, .bullet = true, .sleepThreshold = 3.0F});
    ball.addCircle(20.0F, {.density = 2.0F, .friction = 0.3F, .restitution = 0.8F});
    const float mass = ball.getMass();
    // Box2D holds a rotation as an approximate cosine and sine, so the moved body matches the rotation the body had, not the angle it was made with.
    const float rotation = ball.getRotation();

    Body moved = ball.moveToWorld(space);
    EXPECT_FALSE(ball.isValid());
    ASSERT_TRUE(moved.isValid());
    EXPECT_EQ(moved.getWorld(), &space);
    EXPECT_EQ(ground.getStats().bodies, 0);
    EXPECT_NEAR(moved.getPosition().x, 100.0F, 1e-3F);
    EXPECT_NEAR(moved.getPosition().y, -50.0F, 1e-3F);
    EXPECT_FLOAT_EQ(moved.getRotation(), rotation);
    EXPECT_NEAR(moved.getVelocity().x, 300.0F, 1e-2F);
    EXPECT_NEAR(moved.getVelocity().y, -120.0F, 1e-2F);
    EXPECT_NEAR(moved.getAngularVelocity(), 2.0F, 1e-4F);
    EXPECT_FLOAT_EQ(moved.getLinearDamping(), 0.25F);
    EXPECT_FLOAT_EQ(moved.getAngularDamping(), 0.5F);
    EXPECT_FLOAT_EQ(moved.getGravityScale(), 1.5F);
    EXPECT_TRUE(moved.isBullet());
    EXPECT_NEAR(moved.getSleepThreshold(), 3.0F, 1e-3F);
    EXPECT_NEAR(moved.getMass(), mass, mass * 1e-4F);

    const std::vector<Shape> shapes = moved.getShapes();
    ASSERT_EQ(shapes.size(), 1U);
    EXPECT_EQ(shapes[0].getKind(), Shape::Kind::Circle);
    EXPECT_NEAR(shapes[0].getRadius(), 20.0F, 1e-3F);
    EXPECT_FLOAT_EQ(shapes[0].getFriction(), 0.3F);
    EXPECT_FLOAT_EQ(shapes[0].getRestitution(), 0.8F);

    // The world without gravity no longer pulls it down, so only the damping slows it.
    moved.setLinearDamping(0.0F);
    simulate(space, 1.0F);
    EXPECT_NEAR(moved.getVelocity().y, -120.0F, 1.0F);
}

TEST_F(BodyTransferTest, EveryKindOfShapeGoesAcrossWithItsMaterialFilterAndBehavior) {
    Body crate = ground.createBody({.type = Body::Type::Kinematic, .position = {0.0F, 0.0F}});
    crate.addBox({80.0F, 40.0F}, {.filter = {.category = 4, .mask = 6, .group = -2}, .rotation = 0.3F, .tangentSpeed = 50.0F});
    crate.addCapsule({-30.0F, 30.0F}, {30.0F, 30.0F}, 10.0F, {.sensor = true, .sensorEvents = true});
    crate.addSegment({-40.0F, -40.0F}, {40.0F, -40.0F}, {.oneWay = math::Vec2{0.0F, -1.0F}});
    crate.addPolygon(std::vector<math::Vec2>{{0.0F, 0.0F}, {60.0F, 0.0F}, {60.0F, 60.0F}, {30.0F, 20.0F}, {0.0F, 60.0F}}, {.contactEvents = false, .hitEvents = false});
    crate.addChain(std::vector<math::Vec2>{{-100.0F, 100.0F}, {0.0F, 120.0F}, {100.0F, 100.0F}}, false, {.friction = 0.9F});
    crate.addChain(std::vector<math::Vec2>{{-50.0F, 200.0F}, {50.0F, 200.0F}, {50.0F, 260.0F}, {-50.0F, 260.0F}}, true, {.rollingResistance = 0.2F});
    const std::vector<Shape::Outline> before = crate.getOutlines();
    const std::size_t count = crate.getShapes().size();

    Body moved = crate.moveToWorld(space);
    EXPECT_EQ(moved.getType(), Body::Type::Kinematic);
    EXPECT_EQ(moved.getShapes().size(), count);

    // The outlines of the shapes, the chains joined as before, land where they were.
    const std::vector<Shape::Outline> after = moved.getOutlines();
    ASSERT_EQ(after.size(), before.size());
    for (std::size_t index = 0; index < before.size(); ++index) {
        ASSERT_EQ(after[index].points.size(), before[index].points.size()) << "Outline " << index;
        EXPECT_EQ(after[index].closed, before[index].closed) << "Outline " << index;
        for (std::size_t point = 0; point < before[index].points.size(); ++point) {
            EXPECT_NEAR(math::Vec2::distance(after[index].points[point], before[index].points[point]), 0.0F, 1e-2F) << "Outline " << index << ", point " << point;
        }
    }

    bool box = false;
    bool sensor = false;
    bool oneWay = false;
    bool quiet = false;
    bool grippy = false;
    bool rolling = false;
    for (const Shape& shape : moved.getShapes()) {
        box = box || (shape.getFilter().category == 4 && shape.getFilter().mask == 6 && shape.getFilter().group == -2 && shape.getTangentSpeed() == 50.0F);
        sensor = sensor || (shape.isSensor() && shape.getKind() == Shape::Kind::Capsule);
        oneWay = oneWay || (shape.getOneWay() && shape.getOneWay()->y == -1.0F);
        quiet = quiet || (shape.getKind() == Shape::Kind::Polygon && !shape.hasContactEvents() && !shape.hasHitEvents());
        grippy = grippy || (shape.getKind() == Shape::Kind::ChainSegment && shape.getFriction() == 0.9F);
        rolling = rolling || (shape.getKind() == Shape::Kind::ChainSegment && shape.getRollingResistance() == 0.2F);
    }
    EXPECT_TRUE(box);
    EXPECT_TRUE(sensor);
    EXPECT_TRUE(oneWay);
    EXPECT_TRUE(quiet);
    EXPECT_TRUE(grippy);
    EXPECT_TRUE(rolling);
}

TEST_F(BodyTransferTest, KeepsSizeSpeedAndHandSetMassInAWorldOfAnotherScale) {
    World fine({.gravity = {}, .pixelsPerMeter = 16.0F});
    Body block = ground.createBody({.velocity = {200.0F, 0.0F}});
    block.addBox({64.0F, 32.0F});
    block.setMassData({.mass = 5.0F, .center = {10.0F, 0.0F}, .inertia = 9000.0F});

    Body moved = block.moveToWorld(fine);
    const std::vector<math::Vec2> corners = moved.getShapes()[0].getPoints();
    EXPECT_NEAR(corners[0].x, -32.0F, 1e-3F);
    EXPECT_NEAR(corners[0].y, -16.0F, 1e-3F);
    EXPECT_NEAR(moved.getVelocity().x, 200.0F, 1e-2F);
    EXPECT_NEAR(moved.getMass(), 5.0F, 1e-4F);
    EXPECT_NEAR(moved.getCenterOfMass().x, 10.0F, 1e-3F);
    EXPECT_NEAR(moved.getInertia(), 9000.0F, 1.0F);

    simulate(fine, 1.0F);
    EXPECT_NEAR(moved.getPosition().x, 200.0F, 1.0F);
}

TEST_F(BodyTransferTest, JointsEndAndTheSameWorldIsRefused) {
    Body anchor = ground.createBody({.type = Body::Type::Static});
    anchor.addBox({10.0F, 10.0F});
    Body swing = ground.createBody({.position = {50.0F, 0.0F}});
    swing.addCircle(10.0F);
    const Joint joint = ground.createJoint(Joint::Type::Revolute, anchor, swing, {.anchorA = {0.0F, 0.0F}});

    EXPECT_THROW((void)swing.moveToWorld(ground), std::invalid_argument);
    EXPECT_TRUE(swing.isValid());

    const Body moved = swing.moveToWorld(space);
    EXPECT_FALSE(joint.isValid());
    EXPECT_TRUE(anchor.isValid());
    EXPECT_EQ(moved.getWorld(), &space);
    EXPECT_THROW((void)swing.moveToWorld(space), std::logic_error);
}

} // namespace haylen::physics2d
