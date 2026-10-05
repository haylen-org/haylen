#include <gtest/gtest.h>

#include <algorithm>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "haylen/2d/physics/Grabber.hpp"
#include "haylen/2d/physics/Ragdoll.hpp"
#include "haylen/2d/physics/Rope.hpp"
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
    // Nine joints between segments, one to the hook and the slack joint that limits the length.
    EXPECT_EQ(rope.getJoints().size(), 11U);
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

    // A rope that fails halfway leaves no bodies behind.
    World other;
    const Body stranger = other.createBody();
    EXPECT_THROW((void)Rope::create(world, {.start = {0.0F, 0.0F}, .end = {100.0F, 0.0F}, .segments = 4, .endBody = stranger}), std::invalid_argument);
    EXPECT_THROW((void)Rope::create(world, {.start = {0.0F, 0.0F}, .end = {100.0F, 0.0F}, .density = -1.0F, .pinStart = true}), std::invalid_argument);
    EXPECT_EQ(world.getBodyCount(), 1U);
}

TEST_F(AssemblyTest, BridgesHoldLoadsBetweenPinnedEnds) {
    Rope bridge = Rope::createBridge(world, {.start = {0.0F, 100.0F}, .end = {300.0F, 100.0F}, .segments = 12, .thickness = 8.0F});
    // Eleven joints between planks, one at each end, the length limit and two anchors of their own.
    EXPECT_EQ(bridge.getJoints().size(), 14U);
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
    Ragdoll ragdoll = Ragdoll::create(world, {.position = {0.0F, 100.0F}, .height = 128.0F, .stiffness = 0.3F});
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
    EXPECT_THROW((void)Ragdoll::create(world, {.filter = {.group = 1}}), std::invalid_argument);
    EXPECT_THROW((void)Ragdoll::create(world, {.stiffness = 2.0F}), std::invalid_argument);
    EXPECT_THROW((void)Ragdoll::create(world, {.density = -1.0F}), std::invalid_argument);
    EXPECT_EQ(world.getBodyCount(), 1U);
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

TEST_F(AssemblyTest, RopesWithALengthLimitHoldHeavyLoads) {
    // A load a hundred times the mass of a segment stretches a plain chain, and the length limit holds it at the length of the rope.
    for (const bool limited : {false, true}) {
        World chain;
        Body load = chain.createBody({.position = {0.0F, 400.0F}});
        load.addCircle(20.0F, {.density = 40.0F});
        const Rope rope = Rope::create(chain, {.start = {0.0F, 0.0F}, .end = {0.0F, 380.0F}, .segments = 20, .pinStart = true, .limitLength = limited, .endBody = load});
        for (int step = 0; step < 300; ++step) {
            chain.step(1.0F / 60.0F);
        }
        const float stretch = load.getPosition().y - 400.0F;
        if (limited) {
            EXPECT_LT(stretch, 4.0F);
        } else {
            EXPECT_GT(stretch, 8.0F);
        }
    }
}

TEST_F(AssemblyTest, RagdollsComputeJointStiffnessFromTheirSize) {
    // A stiff figure of any size and scale bends its joints with torques that grow with its weight, and a limp one keeps none.
    const Ragdoll small = Ragdoll::create(world, {.height = 100.0F, .stiffness = 0.5F});
    const Ragdoll tall = Ragdoll::create(world, {.position = {400.0F, 0.0F}, .height = 200.0F, .stiffness = 0.5F, .filter = {.group = -2}});
    const Ragdoll limp = Ragdoll::create(world, {.position = {800.0F, 0.0F}, .stiffness = 0.0F, .filter = {.group = -3}});
    const float smallTorque = small.getJoints()[1].getMaxMotorTorque();
    const float tallTorque = tall.getJoints()[1].getMaxMotorTorque();
    EXPECT_GT(smallTorque, 0.0F);
    // Mass grows with the area and the lever with the height, so the torque grows eight times when the height doubles.
    EXPECT_NEAR(tallTorque / smallTorque, 8.0F, 0.1F);
    EXPECT_FALSE(limp.getJoints()[1].isMotorEnabled());
    EXPECT_NEAR(small.getMass() * 4.0F, tall.getMass(), tall.getMass() * 0.01F);
}

TEST_F(AssemblyTest, GrabbersDragRagdollsOverSteps) {
    // A ragdoll lying in front of a step follows a drag by one hand up onto the step and over it, the regression of figures stuck on stairs.
    ground({0.0F, 300.0F}, {2000.0F, 40.0F});
    ground({300.0F, 220.0F}, {300.0F, 120.0F});
    Ragdoll ragdoll = Ragdoll::create(world, {.position = {0.0F, 200.0F}, .height = 160.0F});
    simulate(2.0F);

    Grabber grabber(world, {.pickRadius = 12.0F});
    const Body hand = ragdoll.getBody(Ragdoll::Part::LowerArmRight);
    ASSERT_TRUE(grabber.grab(hand.getPosition()).has_value());
    EXPECT_GT(grabber.getForce(), 0.0F);
    for (int step = 0; step < 240; ++step) {
        const float progress = std::min(1.0F, static_cast<float>(step) / 120.0F);
        grabber.moveTo(math::Vec2::lerp(hand.getPosition(), {320.0F, 0.0F}, progress));
        world.step(1.0F / 60.0F);
    }
    grabber.release();
    simulate(2.0F);

    // The whole figure lies on top of the step.
    for (const Body& part : ragdoll.getBodies()) {
        EXPECT_GT(part.getPosition().x, 150.0F);
        EXPECT_LT(part.getPosition().y, 162.0F);
    }
}

TEST_F(AssemblyTest, GrabbersLiftWholeAssemblies) {
    // A grabber holding the end of a chain of ten links lifts the whole chain, because it pulls with the weight of everything joined to the link.
    Body previous = world.createBody({.position = {0.0F, 0.0F}});
    previous.addBox({20.0F, 8.0F});
    for (int link = 1; link < 10; ++link) {
        Body body = world.createBody({.position = {static_cast<float>(link) * 20.0F, 0.0F}});
        body.addBox({20.0F, 8.0F});
        (void)world.createJoint(Joint::Type::Revolute, previous, body, {.anchorA = {static_cast<float>(link) * 20.0F - 10.0F, 0.0F}});
        previous = body;
    }
    Grabber grabber(world, {.pickRadius = 4.0F, .strength = 30.0F});
    ASSERT_TRUE(grabber.grab({0.0F, 0.0F}).has_value());
    EXPECT_NEAR(grabber.getForce(), 30.0F * previous.getMass() * 10.0F * 9.80665F * world.getPixelsPerMeter(), grabber.getForce() * 0.01F);
    grabber.moveTo({0.0F, -300.0F});
    simulate(1.5F);
    EXPECT_LT(previous.getPosition().y, -60.0F);
    EXPECT_NEAR(grabber.getHandle().y, -300.0F, 30.0F);

    // A point far from every body takes nothing, and the release lets go.
    EXPECT_FALSE(grabber.grab({5000.0F, 5000.0F}).has_value());
    EXPECT_FALSE(grabber.isHolding());
    EXPECT_THROW(Grabber(world, {.strength = 0.0F}), std::invalid_argument);
}

TEST_F(AssemblyTest, OneWayPlatformsDoNotSnapBodiesUpFromInside) {
    // A ball that rises halfway into a platform and stops there falls back under it instead of popping up on top.
    Body platform = world.createBody({.type = Body::Type::Static, .position = {0.0F, 0.0F}});
    platform.addBox({200.0F, 40.0F}, {.oneWay = math::Vec2{0.0F, -1.0F}});
    // From 60 units below the top surface at 20 units, a throw that peaks 10 units inside the platform needs this speed.
    const float speed = std::sqrt(2.0F * 980.0F * 50.0F);
    Body ball = world.createBody({.position = {0.0F, 80.0F}, .velocity = {0.0F, -speed}});
    ball.addCircle(8.0F);
    simulate(2.0F);
    EXPECT_GT(ball.getPosition().y, 20.0F);
}

TEST_F(AssemblyTest, OneWayPlatformsCatchFastFalls) {
    // Bodies falling fast land on a thin static one-way platform and on a moving one, wherever they hit it.
    Body platform = world.createBody({.type = Body::Type::Static, .position = {0.0F, 0.0F}});
    platform.addBox({400.0F, 8.0F}, {.oneWay = math::Vec2{0.0F, -1.0F}});
    Body lift = world.createBody({.type = Body::Type::Kinematic, .position = {1000.0F, 0.0F}, .velocity = {0.0F, -40.0F}});
    lift.addBox({400.0F, 8.0F}, {.oneWay = math::Vec2{0.0F, -1.0F}});
    std::vector<Body> fallers;
    for (int index = 0; index < 10; ++index) {
        for (const float x : {-180.0F + static_cast<float>(index) * 40.0F, 820.0F + static_cast<float>(index) * 40.0F}) {
            Body body = world.createBody({.position = {x, -300.0F}, .velocity = {0.0F, 1500.0F}, .bullet = x > 500.0F});
            body.addCapsule({0.0F, -10.0F}, {0.0F, 10.0F}, 8.0F);
            fallers.push_back(body);
        }
    }
    simulate(1.0F);
    for (const Body& body : fallers) {
        EXPECT_LT(body.getPosition().y, -10.0F) << body.getPosition().x;
    }
}

TEST_F(AssemblyTest, DropThroughPassesOnePlatform) {
    // A body that drops through the platform it stands on lands on the next one below.
    for (const float y : {0.0F, 60.0F, 120.0F}) {
        Body platform = world.createBody({.type = Body::Type::Static, .position = {0.0F, y}});
        platform.addBox({200.0F, 8.0F}, {.oneWay = math::Vec2{0.0F, -1.0F}});
    }
    Body hero = world.createBody({.position = {0.0F, -30.0F}, .fixedRotation = true});
    hero.addCapsule({0.0F, -10.0F}, {0.0F, 10.0F}, 8.0F);
    simulate(1.0F);
    EXPECT_NEAR(hero.getPosition().y, -22.0F, 2.0F);

    hero.dropThrough(0.2F);
    simulate(1.0F);
    EXPECT_NEAR(hero.getPosition().y, 38.0F, 2.0F);
    EXPECT_THROW(hero.dropThrough(-1.0F), std::invalid_argument);
}

} // namespace haylen::physics2d
