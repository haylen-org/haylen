#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/SceneManager.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"

namespace haylen {

namespace {

class PhysicsWorldTest : public ::testing::Test {
  protected:
    static void simulate(physics2d::World& world, float seconds) {
        for (float time = 0.0F; time < seconds; time += 1.0F / 60.0F) {
            world.step(1.0F / 60.0F);
        }
    }
};

} // namespace

TEST_F(PhysicsWorldTest, DropsBodiesOntoGroundAndReportsContacts) {
    physics2d::World world({.gravity = {0.0F, 980.0F}, .pixelsPerMeter = 64.0F});
    physics2d::Body ground = world.createBody({.type = physics2d::Body::Type::Static, .position = {0.0F, 400.0F}});
    ground.addBox({1000.0F, 20.0F});
    physics2d::Body crate = world.createBody({.position = {0.0F, 0.0F}});
    crate.addBox({32.0F, 32.0F}, {.density = 2.0F, .friction = 0.4F});

    std::size_t begins = 0;
    std::size_t hits = 0;
    for (int step = 0; step < 120; ++step) {
        world.step(1.0F / 60.0F);
        begins += world.getContactBegins().size();
        hits += world.getContactHits().size();
    }

    EXPECT_NEAR(crate.getPosition().y, 374.0F, 2.0F);
    EXPECT_EQ(begins, 1U);
    EXPECT_GE(hits, 1U);
    EXPECT_EQ(world.getBodyCount(), 2U);
    EXPECT_GT(crate.getMass(), 0.0F);
    EXPECT_EQ(crate.getType(), physics2d::Body::Type::Dynamic);
    EXPECT_EQ(ground.getShapes().size(), 1U);
    EXPECT_EQ(world.getGravity(), math::Vec2(0.0F, 980.0F));

    crate.destroy();
    world.step(1.0F / 60.0F);
    EXPECT_FALSE(crate.isValid());
    EXPECT_EQ(world.getBodyCount(), 1U);
    EXPECT_THROW((void)crate.getPosition(), std::logic_error);
    crate.destroy();
}

TEST_F(PhysicsWorldTest, MovesAndConfiguresBodies) {
    physics2d::World world({.gravity = {}});
    physics2d::Body body = world.createBody({.position = {10.0F, 20.0F}, .rotation = 0.5F, .velocity = {64.0F, 0.0F}, .linearDamping = 0.5F, .angularDamping = 0.25F, .gravityScale = 2.0F, .fixedRotation = true, .bullet = true});
    body.addCircle(16.0F);
    EXPECT_EQ(body.getPosition(), math::Vec2(10.0F, 20.0F));
    EXPECT_NEAR(body.getRotation(), 0.5F, 0.002F);
    EXPECT_EQ(body.getVelocity(), math::Vec2(64.0F, 0.0F));
    EXPECT_FLOAT_EQ(body.getLinearDamping(), 0.5F);
    EXPECT_FLOAT_EQ(body.getAngularDamping(), 0.25F);
    EXPECT_FLOAT_EQ(body.getGravityScale(), 2.0F);
    EXPECT_TRUE(body.isFixedRotation());
    EXPECT_TRUE(body.isBullet());

    body.setLinearDamping(0.0F);
    body.setAngularDamping(0.0F);
    body.setGravityScale(1.0F);
    body.setFixedRotation(false);
    body.setBullet(false);
    body.setTransform({100.0F, 100.0F}, 0.0F);
    body.setVelocity({});
    body.applyImpulse({body.getMass() * 64.0F, 0.0F});
    simulate(world, 1.0F);
    EXPECT_NEAR(body.getPosition().x, 164.0F, 2.0F);

    body.setVelocity({});
    body.applyForce({0.0F, body.getMass() * 640.0F}, body.getPosition());
    body.applyAngularImpulse(0.1F);
    EXPECT_GT(body.getAngularVelocity(), 0.0F);
    body.setAngularVelocity(0.0F);
    EXPECT_EQ(body.getAngularVelocity(), 0.0F);
    body.applyTorque(1.0F);
    world.step(0.1F);
    EXPECT_GT(body.getVelocity().y, 0.0F);
    EXPECT_GT(body.getAngularVelocity(), 0.0F);

    body.setType(physics2d::Body::Type::Kinematic);
    EXPECT_EQ(body.getType(), physics2d::Body::Type::Kinematic);
    body.setEnabled(false);
    EXPECT_FALSE(body.isEnabled());
    body.setEnabled(true);
    body.setAwake(false);
    EXPECT_FALSE(body.isAwake());
    world.setGravity({0.0F, 10.0F});
    EXPECT_EQ(world.getGravity(), math::Vec2(0.0F, 10.0F));
}

TEST_F(PhysicsWorldTest, UsesWorldUnitsForTorquesAndMotors) {
    // A disc spins and a slider moves by the same amounts at every scale, because inertia, torque and force all use world units.
    for (const float scale : {32.0F, 64.0F}) {
        physics2d::World world({.gravity = {}, .pixelsPerMeter = scale});
        physics2d::Body disc = world.createBody();
        disc.addCircle(32.0F);
        const float discInertia = 0.5F * disc.getMass() * 32.0F * 32.0F;
        disc.applyAngularImpulse(discInertia * 2.0F);
        EXPECT_NEAR(disc.getAngularVelocity(), 2.0F, 0.01F);
        disc.applyTorque(discInertia * 4.0F);

        const physics2d::Body anchor = world.createBody({.type = physics2d::Body::Type::Static});
        physics2d::Body wheel = world.createBody({.position = {200.0F, 0.0F}});
        wheel.addCircle(32.0F);
        const float wheelInertia = 0.5F * wheel.getMass() * 32.0F * 32.0F;
        world.createJoint(physics2d::Joint::Type::Revolute, anchor, wheel, {.anchorA = {200.0F, 0.0F}, .enableMotor = true, .motorSpeed = 100.0F, .maxMotorTorque = wheelInertia * 4.0F});
        physics2d::Body slider = world.createBody({.position = {0.0F, 200.0F}});
        slider.addBox({32.0F, 32.0F});
        world.createJoint(physics2d::Joint::Type::Prismatic, anchor, slider, {.anchorA = {0.0F, 200.0F}, .enableMotor = true, .motorSpeed = 10000.0F, .maxMotorForce = slider.getMass() * 60.0F});

        world.step(0.25F);
        EXPECT_NEAR(disc.getAngularVelocity(), 3.0F, 0.01F) << scale;
        EXPECT_NEAR(wheel.getAngularVelocity(), 1.0F, 0.02F) << scale;
        EXPECT_NEAR(slider.getVelocity().x, 15.0F, 0.3F) << scale;
    }
}

TEST_F(PhysicsWorldTest, SensesQueriesAndCastsRays) {
    physics2d::World world({.gravity = {0.0F, 980.0F}});
    physics2d::Body zone = world.createBody({.type = physics2d::Body::Type::Static, .position = {0.0F, 200.0F}});
    const physics2d::Shape sensor = zone.addBox({200.0F, 40.0F}, {.sensor = true});
    physics2d::Body wall = world.createBody({.type = physics2d::Body::Type::Static, .position = {300.0F, 0.0F}});
    const physics2d::Shape wallShape = wall.addBox({20.0F, 400.0F}, {.filter = {.category = 2}});
    physics2d::Body ball = world.createBody({.position = {0.0F, 0.0F}});
    ball.addCircle(8.0F);

    std::size_t sensed = 0;
    std::size_t left = 0;
    for (int step = 0; step < 120; ++step) {
        world.step(1.0F / 60.0F);
        for (const physics2d::SensorEvent& event : world.getSensorBegins()) {
            EXPECT_EQ(event.sensor, sensor);
            EXPECT_EQ(event.visitor.getBody(), ball);
            ++sensed;
        }
        left += world.getSensorEnds().size();
    }
    EXPECT_EQ(sensed, 1U);
    EXPECT_EQ(left, 1U);
    EXPECT_TRUE(sensor.isSensor());

    const std::optional<physics2d::RaycastHit> hit = world.raycast({0.0F, -100.0F}, {600.0F, -100.0F});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->shape, wallShape);
    EXPECT_NEAR(hit->point.x, 290.0F, 0.5F);
    EXPECT_NEAR(hit->normal.x, -1.0F, 0.001F);
    EXPECT_FALSE(world.raycast({0.0F, -100.0F}, {600.0F, -100.0F}, {.mask = 1}).has_value());
    EXPECT_FALSE(world.raycast({0.0F, -500.0F}, {10.0F, -500.0F}).has_value());

    EXPECT_EQ(world.queryRect({280.0F, -10.0F, 40.0F, 20.0F}).size(), 1U);
    EXPECT_EQ(world.queryCircle({300.0F, 0.0F}, 5.0F).front(), wallShape);
    EXPECT_EQ(world.queryPoint({300.0F, 0.0F}).size(), 1U);
    EXPECT_TRUE(world.queryPoint({500.0F, 500.0F}).empty());
    EXPECT_NEAR(wallShape.getBounds().width, 20.0F, 4.0F);

    physics2d::Shape moved = wallShape;
    moved.setFilter({.category = 4, .mask = 1});
    EXPECT_EQ(moved.getFilter().category, 4U);
    EXPECT_EQ(moved.getFilter().mask, 1U);
}

TEST_F(PhysicsWorldTest, BuildsPolygonsChainsAndCapsules) {
    physics2d::World world;
    physics2d::Body body = world.createBody({.type = physics2d::Body::Type::Static});

    const std::vector<math::Vec2> square{{0.0F, 0.0F}, {32.0F, 0.0F}, {32.0F, 32.0F}, {0.0F, 32.0F}};
    EXPECT_EQ(body.addPolygon(square).size(), 1U);
    const std::vector<math::Vec2> dent{{0.0F, 0.0F}, {40.0F, 0.0F}, {20.0F, 10.0F}, {40.0F, 40.0F}, {0.0F, 40.0F}};
    EXPECT_EQ(body.addPolygon(dent, {.offset = {100.0F, 0.0F}, .rotation = 0.1F}).size(), 2U);

    const std::vector<math::Vec2> outline{{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}};
    const std::vector<physics2d::Shape> chain = body.addChain(outline, true);
    EXPECT_EQ(chain.size(), 4U);
    EXPECT_THROW(physics2d::Shape(chain.front()).destroy(), std::logic_error);

    physics2d::Shape capsule = body.addCapsule({0.0F, 0.0F}, {0.0F, 50.0F}, 10.0F);
    physics2d::Shape segment = body.addSegment({0.0F, 0.0F}, {50.0F, 0.0F});
    EXPECT_TRUE(capsule.isValid());
    capsule.destroy();
    EXPECT_FALSE(capsule.isValid());
    EXPECT_THROW((void)capsule.getBounds(), std::logic_error);
    segment.destroy();

    EXPECT_THROW(body.addBox({0.0F, 10.0F}), std::invalid_argument);
    EXPECT_THROW(body.addCircle(0.0F), std::invalid_argument);
    EXPECT_THROW(body.addCapsule({}, {1.0F, 0.0F}, 0.0F), std::invalid_argument);
    EXPECT_THROW(body.addPolygon(std::vector<math::Vec2>{{0.0F, 0.0F}, {1.0F, 0.0F}}), std::invalid_argument);
    EXPECT_THROW(body.addPolygon(std::vector<math::Vec2>{{0.0F, 0.0F}, {1.0F, 0.0F}, {2.0F, 0.0F}}), std::invalid_argument);
    EXPECT_THROW(body.addChain(std::vector<math::Vec2>{{0.0F, 0.0F}, {1.0F, 0.0F}, {2.0F, 0.0F}}, false), std::invalid_argument);
    EXPECT_THROW(physics2d::World({.pixelsPerMeter = 0.0F}), std::invalid_argument);
}

TEST_F(PhysicsWorldTest, RejectsWhatBox2DCannotHoldBeforeCreatingIt) {
    physics2d::World world;
    physics2d::Body body = world.createBody();
    const std::vector<math::Vec2> outline{{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}};

    EXPECT_THROW(body.addSegment({10.0F, 0.0F}, {10.1F, 0.0F}), std::invalid_argument);
    EXPECT_THROW(body.addBox({10.0F, 10.0F}, {.density = -1.0F}), std::invalid_argument);
    EXPECT_THROW(body.addCircle(5.0F, {.friction = std::numeric_limits<float>::quiet_NaN()}), std::invalid_argument);
    EXPECT_THROW(body.addBox({10.0F, 10.0F}, {.oneWay = math::Vec2{}}), std::invalid_argument);
    EXPECT_THROW(body.addChain(outline, true, {.restitution = -0.5F}), std::invalid_argument);
    EXPECT_THROW(body.addChain(outline, true, {.oneWay = math::Vec2{}}), std::invalid_argument);
    EXPECT_TRUE(body.getShapes().empty());
    EXPECT_TRUE(body.addSegment({10.0F, 0.0F}, {11.0F, 0.0F}).isValid());

    EXPECT_THROW(body.setLinearDamping(-1.0F), std::invalid_argument);
    EXPECT_THROW(body.setAngularDamping(std::numeric_limits<float>::infinity()), std::invalid_argument);
    EXPECT_THROW(world.createBody({.linearDamping = -0.1F}), std::invalid_argument);
    EXPECT_EQ(world.getBodyCount(), 1U);

    EXPECT_THROW((void)world.queryRect({0.0F, 0.0F, -10.0F, 10.0F}), std::invalid_argument);
    EXPECT_EQ(world.queryRect({0.0F, 0.0F, 20.0F, 0.0F}).size(), 1U);
}

TEST_F(PhysicsWorldTest, ReportsWhenBox2DHoldsTooManyWorlds) {
    std::vector<std::unique_ptr<physics2d::World>> worlds;
    EXPECT_THROW(for (int index = 0; index < 1000; ++index) { worlds.push_back(std::make_unique<physics2d::World>()); }, std::runtime_error);

    // A world that goes makes room for another one.
    worlds.pop_back();
    EXPECT_NO_THROW(worlds.push_back(std::make_unique<physics2d::World>()));
}

// A new world reuses the slot of a destroyed one and hands out the same body, shape and joint ids, so a handle checks the generation of its world as well.
TEST_F(PhysicsWorldTest, DetectsHandlesOfADestroyedWorldWhoseSlotANewWorldReuses) {
    physics2d::Body staleBody;
    physics2d::Shape staleShape;
    physics2d::Joint staleJoint;
    {
        physics2d::World old;
        staleBody = old.createBody();
        staleShape = staleBody.addBox({10.0F, 10.0F});
        staleJoint = old.createJoint(physics2d::Joint::Type::Weld, staleBody, old.createBody({.position = {20.0F, 0.0F}}));
    }

    physics2d::World reused;
    physics2d::Body body = reused.createBody();
    const physics2d::Shape shape = body.addBox({10.0F, 10.0F});
    const physics2d::Joint joint = reused.createJoint(physics2d::Joint::Type::Weld, body, reused.createBody({.position = {20.0F, 0.0F}}));
    EXPECT_EQ(body.getId(), staleBody.getId());
    EXPECT_EQ(shape.getId(), staleShape.getId());
    EXPECT_EQ(joint.getId(), staleJoint.getId());

    EXPECT_TRUE(body.isValid());
    EXPECT_FALSE(staleBody.isValid());
    EXPECT_FALSE(staleShape.isValid());
    EXPECT_FALSE(staleJoint.isValid());
    EXPECT_NE(staleBody, body);
    EXPECT_NE(staleShape, shape);
    for (const auto& [use, message] : std::array<std::pair<std::function<void()>, std::string>, 3>{{
             {[&staleBody] { (void)staleBody.getPosition(); }, "The physics world of the body was destroyed."},
             {[&staleShape] { (void)staleShape.getBounds(); }, "The physics world of the shape was destroyed."},
             {[&staleJoint] { (void)staleJoint.getMotorSpeed(); }, "The physics world of the joint was destroyed."},
         }}) {
        try {
            use();
            ADD_FAILURE() << message;
        } catch (const std::logic_error& error) {
            EXPECT_EQ(std::string(error.what()), message);
        }
    }

    // Destroying through a stale handle leaves the new world alone.
    staleBody.destroy();
    staleShape.destroy();
    staleJoint.destroy();
    EXPECT_TRUE(body.isValid() && shape.isValid() && joint.isValid());
    EXPECT_EQ(reused.getBodyCount(), 2U);
}

TEST_F(PhysicsWorldTest, ReportsTheGeometryAndOutlinesOfShapes) {
    physics2d::World world;
    physics2d::Body body = world.createBody({.type = physics2d::Body::Type::Static, .position = {100.0F, 50.0F}, .rotation = std::numbers::pi_v<float> * 0.5F});
    // clang-format off
    const auto expectInWorld = [](math::Vec2 local, math::Vec2 placed) {
        EXPECT_NEAR(placed.x, 100.0F - local.y, 1e-3F);
        EXPECT_NEAR(placed.y, 50.0F + local.x, 1e-3F);
    };
    // clang-format on

    // A quarter turn maps the body point (x, y) to (100 - y, 50 + x) in the world.
    const physics2d::Shape box = body.addBox({40.0F, 20.0F}, {.offset = {10.0F, 0.0F}});
    const std::vector<math::Vec2> corners = box.getPoints();
    ASSERT_EQ(corners.size(), 4U);
    EXPECT_EQ(box.getKind(), physics2d::Shape::Kind::Polygon);
    EXPECT_EQ(box.getRadius(), 0.0F);
    for (std::size_t index = 0; index < corners.size(); ++index) {
        EXPECT_NEAR(std::abs(corners[index].x - 10.0F), 20.0F, 1e-3F);
        EXPECT_NEAR(std::abs(corners[index].y), 10.0F, 1e-3F);
        expectInWorld(corners[index], box.getWorldPoints()[index]);
    }
    EXPECT_TRUE(box.getOutline().closed);
    EXPECT_EQ(box.getOutline().points.size(), 4U);

    const physics2d::Shape circle = body.addCircle(12.0F, {.offset = {0.0F, 30.0F}});
    EXPECT_EQ(circle.getKind(), physics2d::Shape::Kind::Circle);
    EXPECT_EQ(circle.getPoints(), std::vector<math::Vec2>({{0.0F, 30.0F}}));
    EXPECT_FLOAT_EQ(circle.getRadius(), 12.0F);
    const physics2d::Shape::Outline round = circle.getOutline();
    EXPECT_TRUE(round.closed);
    EXPECT_EQ(round.points.size(), 16U);
    for (const math::Vec2 point : round.points) {
        EXPECT_NEAR(math::Vec2::distance(point, {70.0F, 50.0F}), 12.0F, 1e-3F);
    }

    const physics2d::Shape capsule = body.addCapsule({0.0F, -10.0F}, {0.0F, 10.0F}, 6.0F);
    EXPECT_EQ(capsule.getKind(), physics2d::Shape::Kind::Capsule);
    EXPECT_FLOAT_EQ(capsule.getRadius(), 6.0F);
    expectInWorld({0.0F, 10.0F}, capsule.getWorldPoints()[1]);

    // Every point of the outline of a capsule lies its radius away from the segment between its centers, which runs from (90, 50) to (110, 50) in the world.
    const physics2d::Shape::Outline pill = capsule.getOutline();
    EXPECT_TRUE(pill.closed);
    EXPECT_EQ(pill.points.size(), 18U);
    for (const math::Vec2 point : pill.points) {
        EXPECT_NEAR(math::Vec2::distance(point, {std::clamp(point.x, 90.0F, 110.0F), 50.0F}), 6.0F, 1e-3F);
    }

    const physics2d::Shape segment = body.addSegment({-50.0F, 0.0F}, {50.0F, 0.0F});
    EXPECT_EQ(segment.getKind(), physics2d::Shape::Kind::Segment);
    EXPECT_FALSE(segment.getOutline().closed);
    expectInWorld({50.0F, 0.0F}, segment.getOutline().points[1]);
    EXPECT_EQ(body.getOutlines().size(), 4U);

    // Chains join their segments into one outline, where an open chain leaves out the ghost points at its ends.
    physics2d::Body ground = world.createBody({.type = physics2d::Body::Type::Static});
    const std::vector<physics2d::Shape> loop = ground.addChain(std::vector<math::Vec2>{{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}}, true);
    (void)ground.addChain(std::vector<math::Vec2>{{0.0F, 200.0F}, {50.0F, 200.0F}, {100.0F, 210.0F}, {150.0F, 200.0F}, {200.0F, 200.0F}}, false);
    EXPECT_EQ(loop.front().getKind(), physics2d::Shape::Kind::ChainSegment);
    EXPECT_EQ(loop.front().getPoints().size(), 2U);
    const std::vector<physics2d::Shape::Outline> outlines = ground.getOutlines();
    ASSERT_EQ(outlines.size(), 2U);
    const physics2d::Shape::Outline& closed = outlines[0].closed ? outlines[0] : outlines[1];
    const physics2d::Shape::Outline& open = outlines[0].closed ? outlines[1] : outlines[0];
    EXPECT_EQ(closed.points, std::vector<math::Vec2>({{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}}));
    EXPECT_FALSE(open.closed);
    EXPECT_EQ(open.points, std::vector<math::Vec2>({{50.0F, 200.0F}, {100.0F, 210.0F}, {150.0F, 200.0F}}));

    EXPECT_EQ(physics2d::Shape::kindName(physics2d::Shape::Kind::ChainSegment), "chainSegment");
    EXPECT_EQ(physics2d::Shape::kindFromName("capsule"), physics2d::Shape::Kind::Capsule);
    EXPECT_FALSE(physics2d::Shape::kindFromName("triangle").has_value());
}

TEST_F(PhysicsWorldTest, ConnectsBodiesWithJoints) {
    physics2d::World world({.gravity = {0.0F, 980.0F}});
    physics2d::Body anchor = world.createBody({.type = physics2d::Body::Type::Static, .position = {0.0F, 0.0F}});
    anchor.addCircle(4.0F);

    std::vector<physics2d::Joint> joints;
    std::vector<physics2d::Body> bobs;
    for (const physics2d::Joint::Type type : {physics2d::Joint::Type::Distance, physics2d::Joint::Type::Revolute, physics2d::Joint::Type::Prismatic, physics2d::Joint::Type::Weld, physics2d::Joint::Type::Wheel, physics2d::Joint::Type::Mouse, physics2d::Joint::Type::Motor}) {
        physics2d::Body bob = world.createBody({.position = {100.0F, 0.0F}});
        bob.addCircle(8.0F, {.filter = {.group = -1}});
        bobs.push_back(bob);
        const bool limited = type != physics2d::Joint::Type::Distance && type != physics2d::Joint::Type::Revolute;
        joints.push_back(world.createJoint(type, anchor, bob, {.anchorA = {0.0F, 0.0F}, .anchorB = {100.0F, 0.0F}, .enableLimit = limited, .lower = -1.0F, .upper = 1.0F, .enableMotor = limited, .maxMotorForce = 10.0F, .maxMotorTorque = 10.0F, .axis = {0.0F, 1.0F}}));
        EXPECT_TRUE(joints.back().isValid());
    }
    simulate(world, 0.5F);

    EXPECT_NEAR(bobs[0].getPosition().getLength(), 100.0F, 1.0F);
    EXPECT_NEAR(bobs[1].getPosition().getLength(), 100.0F, 1.0F);
    EXPECT_GT(bobs[1].getPosition().y, 90.0F);

    joints[1].setMotorSpeed(2.0F);
    joints[2].setMotorSpeed(64.0F);
    joints[4].setMotorSpeed(1.0F);
    joints[5].setTarget({50.0F, 50.0F});
    EXPECT_FLOAT_EQ(joints[1].getMotorSpeed(), 2.0F);
    EXPECT_NEAR(joints[2].getMotorSpeed(), 64.0F, 1e-3F);
    EXPECT_FLOAT_EQ(joints[4].getMotorSpeed(), 1.0F);
    EXPECT_NEAR(joints[5].getTarget().x, 50.0F, 1e-3F);
    EXPECT_THROW(joints[0].setMotorSpeed(1.0F), std::logic_error);
    EXPECT_THROW((void)joints[0].getMotorSpeed(), std::logic_error);
    EXPECT_THROW(joints[0].setTarget({}), std::logic_error);
    EXPECT_THROW((void)joints[1].getTarget(), std::logic_error);

    joints[0].destroy();
    EXPECT_FALSE(joints[0].isValid());
    EXPECT_THROW(joints[0].setMotorSpeed(1.0F), std::logic_error);
    joints[0].destroy();

    // A filter joint lets a crate fall through the floor it would otherwise land on.
    physics2d::Body floor = world.createBody({.type = physics2d::Body::Type::Static, .position = {500.0F, 200.0F}});
    floor.addBox({200.0F, 20.0F});
    physics2d::Body ghost = world.createBody({.position = {500.0F, 100.0F}});
    ghost.addBox({20.0F, 20.0F});
    physics2d::Body solid = world.createBody({.position = {560.0F, 100.0F}});
    solid.addBox({20.0F, 20.0F});
    EXPECT_TRUE(world.createJoint(physics2d::Joint::Type::Filter, floor, ghost).isValid());
    simulate(world, 1.0F);
    EXPECT_GT(ghost.getPosition().y, 300.0F);
    EXPECT_LT(solid.getPosition().y, 200.0F);
    EXPECT_EQ(physics2d::Joint::typeFromName("filter"), physics2d::Joint::Type::Filter);

    physics2d::World other;
    const physics2d::Body stranger = other.createBody();
    EXPECT_THROW(world.createJoint(physics2d::Joint::Type::Weld, anchor, stranger), std::invalid_argument);

    // Box2D cannot hold a distance joint between anchors in one place, revolute limits near a half turn or limits in the wrong order.
    EXPECT_THROW(world.createJoint(physics2d::Joint::Type::Distance, anchor, ghost), std::invalid_argument);
    EXPECT_TRUE(world.createJoint(physics2d::Joint::Type::Distance, anchor, ghost, {.length = 1.0F}).isValid());
    EXPECT_THROW(world.createJoint(physics2d::Joint::Type::Revolute, anchor, ghost, {.lower = -3.13F, .upper = 0.0F}), std::invalid_argument);
    EXPECT_THROW(world.createJoint(physics2d::Joint::Type::Revolute, anchor, ghost, {.lower = 0.5F, .upper = -0.5F}), std::invalid_argument);
    EXPECT_THROW(world.createJoint(physics2d::Joint::Type::Prismatic, anchor, ghost, {.lower = 10.0F, .upper = -10.0F}), std::invalid_argument);
    EXPECT_THROW(world.createJoint(physics2d::Joint::Type::Wheel, anchor, ghost, {.lower = 10.0F, .upper = -10.0F}), std::invalid_argument);
    EXPECT_TRUE(world.createJoint(physics2d::Joint::Type::Revolute, anchor, ghost, {.lower = -3.1F, .upper = 3.1F}).isValid());
    EXPECT_EQ(physics2d::Joint::typeFromName("wheel"), physics2d::Joint::Type::Wheel);
    EXPECT_EQ(physics2d::Joint::typeName(physics2d::Joint::Type::Motor), "motor");
    EXPECT_FALSE(physics2d::Joint::typeFromName("rope").has_value());
    EXPECT_EQ(physics2d::Body::typeFromName("kinematic"), physics2d::Body::Type::Kinematic);
    EXPECT_EQ(physics2d::Body::typeName(physics2d::Body::Type::Static), "static");
    EXPECT_FALSE(physics2d::Body::typeFromName("ghost").has_value());
}

TEST_F(PhysicsWorldTest, MeasuresJointAnglesFromThePoseAtCreation) {
    // A tight revolute limit and a prismatic joint both keep a body turned the way it was when they joined it.
    physics2d::World world({.gravity = {}});
    const physics2d::Body anchor = world.createBody({.type = physics2d::Body::Type::Static});
    physics2d::Body hinged = world.createBody({.position = {100.0F, 0.0F}, .rotation = 1.0F, .angularVelocity = 0.5F});
    hinged.addBox({40.0F, 10.0F});
    physics2d::Body slider = world.createBody({.position = {0.0F, 100.0F}, .rotation = 0.8F, .velocity = {20.0F, 0.0F}});
    slider.addBox({20.0F, 20.0F});
    world.createJoint(physics2d::Joint::Type::Revolute, anchor, hinged, {.anchorA = {100.0F, 0.0F}, .enableLimit = true, .lower = -0.1F, .upper = 0.1F});
    world.createJoint(physics2d::Joint::Type::Prismatic, anchor, slider, {.anchorA = {0.0F, 100.0F}});

    simulate(world, 1.0F);
    EXPECT_NEAR(hinged.getRotation(), 1.1F, 0.02F);
    EXPECT_NEAR(slider.getRotation(), 0.8F, 0.01F);
    EXPECT_GT(slider.getPosition().x, 10.0F);
}

TEST_F(PhysicsWorldTest, DrawsDebugShapes) {
    test::EngineFixture fixture;
    physics2d::World world;
    physics2d::Body body = world.createBody({.type = physics2d::Body::Type::Static});
    body.addBox({20.0F, 20.0F});
    body.addCircle(5.0F, {.offset = {30.0F, 0.0F}});
    body.addCapsule({0.0F, 40.0F}, {20.0F, 40.0F}, 4.0F);
    body.addSegment({0.0F, 60.0F}, {20.0F, 60.0F});
    physics2d::Body dynamic = world.createBody({.position = {100.0F, 0.0F}});
    dynamic.addCircle(5.0F);
    world.createJoint(physics2d::Joint::Type::Revolute, body, dynamic, {.anchorA = {100.0F, 0.0F}});

    // clang-format off
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([&world](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(graphics2d::Camera{});
        world.debugDraw(engine.getRenderer2D(), {.layer = 10});
    }));
    // clang-format on
    fixture.frames(1);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().vertices, 0U);
}

TEST(Physics2DLuaTest, SimulatesWorldsFromLua) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics2d = require('haylen.physics2d')
        world = physics2d.newWorld({gravity = {0, 980}, pixelsPerMeter = 64, subSteps = 4})
        ground = world:createBody({type = 'static', x = 0, y = 400})
        ground:addBox(1000, 20, {friction = 0.8, category = 2})
        zone = world:createBody({type = 'static', x = 0, y = 200})
        zone:addBox(200, 40, {sensor = true})
        crate = world:createBody({x = 0, y = 0, fixedRotation = true, linearDamping = 0.1})
        crate:addBox(32, 32, {density = 2, restitution = 0, offsetX = 0, offsetY = 0, rotation = 0})
        crate.data = {name = 'crate'}
        events = {}
        world.onContactBegin = function(a, b, contact) events[#events + 1] = 'begin ' .. (a.data or b.data).name end
        world.onContactEnd = function() events[#events + 1] = 'end' end
        world.onHit = function(a, b, contact) hitSpeed = contact.speed end
        world.onSensorBegin = function(sensor, visitor, shapes) events[#events + 1] = 'sensed ' .. visitor.data.name .. ' ' .. tostring(shapes.sensorShape.sensor) end
        world.onSensorEnd = function() events[#events + 1] = 'left' end
        for i = 1, 120 do world:step(1 / 60) end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return table.concat(events, ',')"), "sensed crate true,left,begin crate");
    EXPECT_EQ(fixture.lua("return math.floor(crate.y + 0.5) .. ' ' .. tostring(hitSpeed > 0) .. ' ' .. world.bodyCount .. ' ' .. world.pixelsPerMeter"), "374 true 3 64.0");
    EXPECT_EQ(fixture.lua("return crate.type .. ' ' .. tostring(crate.valid) .. ' ' .. tostring(crate.world == world) .. ' ' .. tostring(crate.fixedRotation) .. ' ' .. tostring(crate.mass > 0)"), "dynamic true true true true");

    fixture.runLua("crate.x = 50 crate.y = 100 crate.rotation = 0 crate.velocity = {10, 0} crate.angularVelocity = 0 crate.linearDamping = 0 crate.angularDamping = 0 crate.gravityScale = 0 crate.bullet = true crate.awake = true crate.enabled = true");
    EXPECT_EQ(fixture.lua("return crate.position.x .. ' ' .. crate.velocity.x .. ' ' .. crate.gravityScale .. ' ' .. tostring(crate.bullet) .. ' ' .. tostring(crate.awake) .. ' ' .. tostring(crate.enabled)"), "50.0 10.0 0.0 true true true");
    fixture.runLua("crate:applyImpulse(0, -10) crate:applyForce(1, 0) crate:applyForce(1, 0, 50, 100) crate:applyImpulse(1, 0, 50, 100) crate:applyTorque(1) crate:applyAngularImpulse(1) crate:setTransform(0, 0, 0.5) crate.type = 'kinematic'");
    EXPECT_EQ(fixture.lua("return crate.type .. ' ' .. string.format('%.1f', crate.rotation) .. ' ' .. crate.linearDamping .. ' ' .. crate.angularDamping"), "kinematic 0.5 0.0 0.0");

    EXPECT_EQ(fixture.lua("local hit = world:raycast(0, -500, 0, 1000) return hit.body == ground or hit.body == zone or hit.body == crate"), "true");
    EXPECT_EQ(fixture.lua("local hit = world:raycast(0, 300, 0, 1000, {mask = 2}) return hit.body == ground and hit.shape.category == 2 and hit.normalY < 0 and hit.fraction > 0 and hit.x == 0 and hit.y > 0 and hit.normalX == 0"), "true");
    EXPECT_EQ(fixture.lua("return tostring(world:raycast(5000, 0, 6000, 0))"), "nil");
    EXPECT_EQ(fixture.lua("return #world:raycastAll(0, -500, 0, 1000)"), "3");
    EXPECT_EQ(fixture.lua("return #world:queryRect({-10, 390, 20, 20}) .. ' ' .. #world:queryCircle(0, 200, 5) .. ' ' .. #world:queryPoint(0, 400, {category = 1, mask = 2})"), "1 1 1");

    // clang-format off
    fixture.runLua(R"(
        hinge = world:createBody({type = 'static', x = 500, y = 0})
        hinge:addCircle(4)
        bob = world:createBody({x = 600, y = 0})
        local shapes = bob:addPolygon({{0, 0}, {16, 0}, {16, 16}, {0, 16}}, {density = 1})
        bob:addCapsule(0, 0, 0, 20, 4)
        bob:addSegment(0, 0, 10, 0)
        hinge:addChain({{0, 0}, {10, 0}, {10, 10}, {0, 10}}, true)
        joint = world:createJoint('revolute', hinge, bob, {ax = 500, ay = 0, enableMotor = true, motorSpeed = 1, maxMotorTorque = 10, enableLimit = true, lower = -1, upper = 1})
        mouse = world:createJoint('mouse', hinge, bob, {bx = 600, by = 0, hertz = 5, dampingRatio = 0.7, maxMotorForce = 100})
        for i = 1, 10 do world:step(1 / 60) end
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("joint.motorSpeed = 2 mouse.target = {610, 10} return tostring(joint.valid) .. ' ' .. #bob:shapes() .. ' ' .. joint.motorSpeed .. ' ' .. math.floor(mouse.target.x + 0.5)"), "true 3 2.0 610");
    EXPECT_NE(fixture.lua("return joint.target").find("Only mouse joints have a target."), std::string::npos);
    EXPECT_NE(fixture.lua("mouse.motorSpeed = 1").find("Only revolute, prismatic and wheel joints have a motor speed."), std::string::npos);
    EXPECT_EQ(fixture.lua("local s = bob:shapes()[1] s.mask = 1 s.category = 4 s.group = -3 return s.mask .. ' ' .. s.category .. ' ' .. s.group .. ' ' .. tostring(s.body == bob) .. ' ' .. tostring(s == s) .. ' ' .. tostring(s.bounds.width > 0)"), "1 4 -3 true true true");
    EXPECT_EQ(fixture.lua("local s = bob:addCircle(2, {group = 5}) return s.group .. ' ' .. hinge:shapes()[1].group"), "5 0");
    EXPECT_EQ(fixture.lua("local s = bob:shapes()[2] s:destroy() return tostring(s.valid) .. ' ' .. #bob:shapes()"), "false 3");
    EXPECT_EQ(fixture.lua("joint:destroy() return tostring(joint.valid)"), "false");
    EXPECT_EQ(fixture.lua("world.gravity = {0, 10} return world.gravity.y"), "10.0");

    fixture.runLua("require('haylen.scene').push({render = function() require('haylen.graphics2d').beginWorld(require('haylen.graphics2d').newCamera()) world:debugDraw({layer = 5}) end})");
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    EXPECT_EQ(fixture.lua("crate:destroy() crate:destroy() return tostring(crate.valid) .. ' ' .. world.bodyCount"), "false 4");
    EXPECT_NE(fixture.lua("return crate.x").find("The body was destroyed."), std::string::npos);
    EXPECT_NE(fixture.lua("world:createBody({kind = 'dynamic'})").find("Unknown option \"kind\""), std::string::npos);
    EXPECT_NE(fixture.lua("bob:addBox(10, 10, {bounce = 1})").find("Unknown option \"bounce\""), std::string::npos);
    EXPECT_NE(fixture.lua("world:createJoint('rope', hinge, bob)").find("unknown value 'rope'"), std::string::npos);
    EXPECT_NE(fixture.lua("world:createJoint('weld', hinge, bob, {speed = 1})").find("Unknown option \"speed\""), std::string::npos);
    EXPECT_NE(fixture.lua("world:raycast(0, 0, 1, 1, {layer = 1})").find("Unknown option \"layer\""), std::string::npos);
    EXPECT_NE(fixture.lua("physics2d.newWorld({pixelsPerMeter = 0})").find("positive pixels per meter"), std::string::npos);
    EXPECT_NE(fixture.lua("world.onHit = 5").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("bob:addBox(0, 10)").find("positive size"), std::string::npos);
    EXPECT_NE(fixture.lua("bob:addBox(10, 10, {density = -1})").find("A physics shape needs a finite density, friction and restitution of zero or more."), std::string::npos);
    EXPECT_NE(fixture.lua("bob:addSegment(0, 0, 0, 0)").find("A physics segment needs ends more than 0.005 meters apart."), std::string::npos);
    EXPECT_NE(fixture.lua("bob.linearDamping = -1").find("A physics body needs a finite damping of zero or more."), std::string::npos);
    EXPECT_NE(fixture.lua("world:createJoint('distance', hinge, bob)").find("A distance joint needs a length of at least 0.005 meters."), std::string::npos);
    EXPECT_NE(fixture.lua("world:createJoint('revolute', hinge, bob, {lower = 1, upper = 0})").find("A revolute joint needs a lower limit"), std::string::npos);
    EXPECT_NE(fixture.lua("world:queryRect({0, 0, -10, 10})").find("A rectangle query needs a width and height of zero or more."), std::string::npos);
    EXPECT_EQ(fixture.lua("return #bob:shapes()"), "3");

    fixture.runLua("world.onHit = nil world.onContactBegin = function() error('contact failed') end local b = world:createBody({x = 0, y = 384}) b:addBox(10, 10)");
    EXPECT_NE(fixture.lua("for i = 1, 60 do world:step(1 / 60) end").find("contact failed"), std::string::npos);
    EXPECT_EQ(fixture.lua("return type(world.onContactBegin)"), "function");
}

TEST(Physics2DLuaTest, DeliversEveryEventWhenACallbackStepsTheWorld) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics2d = require('haylen.physics2d')
        world = physics2d.newWorld({gravity = {0, 0}})
        local ground = world:createBody({type = 'static', x = 0, y = 0})
        ground:addBox(100, 20)
        local crate = world:createBody({x = 0, y = -12})
        crate:addBox(10, 10)
        local zone = world:createBody({type = 'static', x = 0, y = -28})
        zone:addBox(40, 24, {sensor = true})
        events = {}
        world.onContactBegin = function()
            events[#events + 1] = 'contact'
            if not nested then
                nested = true
                world:step(0)
            end
        end
        world.onSensorBegin = function() events[#events + 1] = 'sensor' end
    )");
    // clang-format on

    // The callback steps the world again, which replaces its event lists, yet the events of the outer step still arrive.
    EXPECT_EQ(fixture.lua("world:step(1 / 60) return table.concat(events, ',')"), "contact,sensor");
}

} // namespace haylen
