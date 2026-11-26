#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "3d/physics/jolt/Backend.hpp"
#include "haylen/3d/physics/Body.hpp"
#include "haylen/3d/physics/Shape.hpp"
#include "haylen/3d/physics/World.hpp"
#include "haylen/math/Math.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::physics3d {

class PhysicsBody3DTest : public ::testing::Test {
  protected:
    static constexpr float kStep = 1.0F / 60.0F;
    static constexpr float kRestTolerance = 0.005F;

    [[nodiscard]] std::shared_ptr<const Shape> make(Shape::Geometry geometry, float density = 1000.0F) const {
        return backend.createShape({.geometry = std::move(geometry), .density = density});
    }

    // A floor whose top lies at zero, a hundred meters wide.
    Body addFloor(World& world, float friction = 0.5F) const {
        return world.createBody({.type = Body::Type::Static, .shape = make(Shape::Box{.size = {100.0F, 1.0F, 100.0F}}), .position = {0.0F, -0.5F, 0.0F}, .friction = friction});
    }

    // A body at rest sits on what holds it, within the overlap that the solver leaves to keep piles steady.
    static void expectResting(float height, float expected) {
        EXPECT_LE(height, expected + kRestTolerance);
        EXPECT_GE(height, expected - WorldSettings{}.penetrationSlop - kRestTolerance);
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
    std::shared_ptr<const Shape> ball = make(Shape::Sphere{.radius = 0.25F});
    std::shared_ptr<const Shape> crate = make(Shape::Box{.size = {1.0F, 1.0F, 1.0F}});
};

TEST_F(PhysicsBody3DTest, LetsASphereFallOntoTheFloorAndRest) {
    World world({}, backend);
    addFloor(world);
    const Body body = world.createBody({.shape = ball, .position = {0.0F, 3.0F, 0.0F}});
    EXPECT_EQ(body.getType(), Body::Type::Dynamic);
    EXPECT_NEAR(body.getMass(), 4.0F / 3.0F * math::Math::kPi * 0.015625F * 1000.0F, 0.5F);

    run(world, 240);
    expectResting(body.getPosition().y, 0.25F);
    EXPECT_FALSE(body.isAwake());
    EXPECT_EQ(world.getStats().bodies, 2);
}

TEST_F(PhysicsBody3DTest, StopsAFastSphereAtAThinWallOnlyWithLinearCast) {
    World world({.gravity = {}}, backend);
    world.createBody({.type = Body::Type::Static, .shape = make(Shape::Box{.size = {0.05F, 4.0F, 4.0F}}), .position = {5.0F, 0.0F, 0.0F}});
    const std::shared_ptr<const Shape> bullet = make(Shape::Sphere{.radius = 0.05F});
    const Body swept = world.createBody({.shape = bullet, .position = {0.0F, 1.0F, 0.0F}, .linearVelocity = {400.0F, 0.0F, 0.0F}, .motionQuality = Body::MotionQuality::LinearCast, .linearDamping = 0.0F});
    const Body jumping = world.createBody({.shape = bullet, .position = {0.0F, -1.0F, 0.0F}, .linearVelocity = {400.0F, 0.0F, 0.0F}, .linearDamping = 0.0F});
    run(world, 6);
    EXPECT_LT(swept.getPosition().x, 5.0F);
    EXPECT_GT(jumping.getPosition().x, 5.0F);
    EXPECT_EQ(swept.getMotionQuality(), Body::MotionQuality::LinearCast);
}

TEST_F(PhysicsBody3DTest, MovesByForcesImpulsesAndTorquesAtTheirPoints) {
    World world({.gravity = {}}, backend);
    Body body = world.createBody({.shape = crate, .mass = 2.0F, .linearDamping = 0.0F, .angularDamping = 0.0F});
    EXPECT_NEAR(body.getMass(), 2.0F, 1.0e-5F);

    body.applyImpulse({4.0F, 0.0F, 0.0F});
    EXPECT_NEAR(body.getLinearVelocity().x, 2.0F, 1.0e-5F);

    body.applyImpulse({0.0F, 0.0F, 1.0F}, math::Vec3{0.5F, 0.0F, 0.0F});
    EXPECT_LT(body.getAngularVelocity().y, 0.0F);

    body.setLinearVelocity({});
    body.setAngularVelocity({});
    body.applyForce({0.0F, 6.0F, 0.0F});
    world.step(kStep);
    EXPECT_NEAR(body.getLinearVelocity().y, 3.0F * kStep, 1.0e-4F);

    body.applyTorque({0.0F, 0.0F, 1.0F});
    body.applyAngularImpulse({0.0F, 0.0F, 1.0F});
    world.step(kStep);
    EXPECT_GT(body.getAngularVelocity().z, 0.0F);
    EXPECT_GT(body.getVelocityAt(body.getWorldCenter() + math::Vec3{1.0F, 0.0F, 0.0F}).y, body.getLinearVelocity().y);
}

TEST_F(PhysicsBody3DTest, KeepsTheAxesItLocksAndTheGravityItScales) {
    World world({}, backend);
    Body sliding = world.createBody({.shape = crate, .linearVelocity = {1.0F, 0.0F, 1.0F}, .locks = {.y = true, .z = true, .rotationX = true}});
    Body floating = world.createBody({.shape = ball, .position = {5.0F, 0.0F, 0.0F}, .gravityScale = 0.0F});
    run(world, 30);
    EXPECT_NEAR(sliding.getPosition().y, 0.0F, 1.0e-5F);
    EXPECT_NEAR(sliding.getPosition().z, 0.0F, 1.0e-5F);
    EXPECT_GT(sliding.getPosition().x, 0.3F);
    EXPECT_EQ(sliding.getMotionLocks(), (Body::MotionLocks{.y = true, .z = true, .rotationX = true}));
    EXPECT_NEAR(floating.getPosition().y, 0.0F, 1.0e-5F);

    floating.setGravityScale(1.0F);
    run(world, 30);
    EXPECT_LT(floating.getPosition().y, -0.5F);

    sliding.setMotionLocks({});
    run(world, 30);
    EXPECT_LT(sliding.getPosition().y, -0.5F);
}

TEST_F(PhysicsBody3DTest, SlowsByItsDampingAndKeepsUnderItsSpeedLimits) {
    World world({.gravity = {}}, backend);
    Body damped = world.createBody({.shape = ball, .linearVelocity = {10.0F, 0.0F, 0.0F}, .linearDamping = 2.0F});
    Body limited = world.createBody({.shape = ball, .position = {0.0F, 5.0F, 0.0F}, .linearVelocity = {20.0F, 0.0F, 0.0F}, .linearDamping = 0.0F, .maxLinearSpeed = 20.0F});
    limited.applyImpulse({limited.getMass() * 30.0F, 0.0F, 0.0F});
    run(world, 60);
    EXPECT_LT(damped.getLinearVelocity().x, 2.0F);
    EXPECT_NEAR(limited.getLinearVelocity().x, 20.0F, 1.0e-3F);

    damped.setAngularDamping(1.5F);
    damped.setMaxAngularSpeed(3.0F);
    damped.setMaxLinearSpeed(4.0F);
    EXPECT_FLOAT_EQ(damped.getAngularDamping(), 1.5F);
    EXPECT_FLOAT_EQ(damped.getMaxAngularSpeed(), 3.0F);
    EXPECT_FLOAT_EQ(damped.getMaxLinearSpeed(), 4.0F);
    EXPECT_EQ(refusal<std::invalid_argument>([&] { limited.setLinearVelocity({30.0F, 0.0F, 0.0F}); }), "The linear velocity of a 3D body is faster than its max linear speed of 20. Raise the max linear speed or give a slower velocity.");
}

TEST_F(PhysicsBody3DTest, TakesTheMassInertiaAndCenterOfMassItIsGiven) {
    World world({.gravity = {}}, backend);
    Body body = world.createBody({.shape = crate, .inertia = math::Vec3{1.0F, 2.0F, 3.0F}, .centerOfMass = math::Vec3{0.0F, -0.4F, 0.0F}});
    EXPECT_NEAR(body.getMass(), 1000.0F, 0.5F);
    EXPECT_NEAR(body.getInertia().y, 2.0F, 1.0e-4F);
    EXPECT_NEAR(body.getCenterOfMass().y, -0.4F, 1.0e-5F);
    EXPECT_NEAR(body.getWorldCenter().y, -0.4F, 1.0e-5F);
    EXPECT_NEAR(body.getPosition().y, 0.0F, 1.0e-5F);

    body.setMass(10.0F);
    EXPECT_NEAR(body.getMass(), 10.0F, 1.0e-4F);
    body.applyImpulse({10.0F, 0.0F, 0.0F});
    EXPECT_NEAR(body.getLinearVelocity().x, 1.0F, 1.0e-4F);

    body.resetMass();
    EXPECT_NEAR(body.getMass(), 1000.0F, 0.5F);
    EXPECT_NEAR(body.getCenterOfMass().y, 0.0F, 1.0e-5F);
    EXPECT_NEAR(body.getInertia().y, 1000.0F / 6.0F, 1.0F);
}

TEST_F(PhysicsBody3DTest, BouncesAndSlidesByTheRulesThatCombineItsMaterial) {
    World world({}, backend);
    addFloor(world, 0.0F);
    const Body bouncing = world.createBody({.shape = ball, .position = {-2.0F, 2.0F, 0.0F}, .restitution = 0.9F, .restitutionCombine = Body::CombineRule::Maximum});
    const Body dull = world.createBody({.shape = ball, .position = {2.0F, 2.0F, 0.0F}, .restitution = 0.9F, .restitutionCombine = Body::CombineRule::Minimum});
    run(world, 45);
    float highest = 0.0F;
    for (int step = 0; step < 60; ++step) {
        world.step(kStep);
        highest = std::max(highest, bouncing.getPosition().y);
        EXPECT_LT(dull.getPosition().y, 0.5F);
    }

    EXPECT_GT(highest, 1.0F);
    EXPECT_EQ(bouncing.getRestitutionCombine(), Body::CombineRule::Maximum);

    World slope({}, backend);
    const math::Quat tilt = math::Quat::fromAxisAngle({0.0F, 0.0F, 1.0F}, 0.3F);
    slope.createBody({.type = Body::Type::Static, .shape = make(Shape::Box{.size = {40.0F, 1.0F, 40.0F}}), .position = {0.0F, -0.5F, 0.0F}, .rotation = tilt, .friction = 1.0F});
    const math::Vec3 above = math::Vec3{0.0F, -0.5F, 0.0F} + tilt.rotate({0.0F, 1.0F, 0.0F});
    const Body gripping = slope.createBody({.shape = crate, .position = above + math::Vec3{0.0F, 0.0F, -3.0F}, .rotation = tilt, .friction = 1.0F});
    const Body slipping = slope.createBody({.shape = crate, .position = above + math::Vec3{0.0F, 0.0F, 3.0F}, .rotation = tilt, .friction = 1.0F, .frictionCombine = Body::CombineRule::Multiply});
    Body sliding = slope.createBody({.shape = crate, .position = above, .rotation = tilt, .friction = 0.0F, .frictionCombine = Body::CombineRule::Minimum});
    run(slope, 60);
    EXPECT_NEAR(gripping.getPosition().x, above.x, 0.05F);
    EXPECT_NEAR(slipping.getPosition().x, above.x, 0.05F);
    EXPECT_LT(sliding.getPosition().x, above.x - 0.5F);

    sliding.setFrictionCombine(Body::CombineRule::Maximum);
    EXPECT_EQ(sliding.getFrictionCombine(), Body::CombineRule::Maximum);
}

TEST_F(PhysicsBody3DTest, LetsSensorsAndBodiesThatAreNotEnabledPassThrough) {
    World world({}, backend);
    addFloor(world);
    Body sensor = world.createBody({.type = Body::Type::Static, .shape = crate, .position = {0.0F, 2.0F, 0.0F}, .sensor = true});
    const Body falling = world.createBody({.shape = ball, .position = {0.0F, 4.0F, 0.0F}});
    Body ghost = world.createBody({.shape = ball, .position = {3.0F, 2.0F, 0.0F}, .enabled = false});
    run(world, 180);
    expectResting(falling.getPosition().y, 0.25F);
    EXPECT_TRUE(sensor.isSensor());
    EXPECT_FALSE(ghost.isEnabled());
    EXPECT_NEAR(ghost.getPosition().y, 2.0F, 1.0e-6F);

    ghost.setEnabled(true);
    run(world, 120);
    expectResting(ghost.getPosition().y, 0.25F);

    sensor.setSensor(false);
    sensor.setCollideWithStatic(true);
    EXPECT_FALSE(sensor.isSensor());
    EXPECT_TRUE(sensor.isCollidingWithStatic());
}

TEST_F(PhysicsBody3DTest, ChangesItsTypeAndSleepsAndWakesOnRequest) {
    World world({}, backend);
    addFloor(world);
    Body body = world.createBody({.type = Body::Type::Static, .shape = crate, .position = {0.0F, 3.0F, 0.0F}});
    run(world, 30);
    EXPECT_NEAR(body.getPosition().y, 3.0F, 1.0e-6F);

    body.setType(Body::Type::Dynamic);
    run(world, 240);
    expectResting(body.getPosition().y, 0.5F);
    EXPECT_FALSE(body.isAwake());

    body.setAwake(true);
    EXPECT_TRUE(body.isAwake());
    body.setSleepEnabled(false);
    run(world, 120);
    EXPECT_TRUE(body.isAwake());
    EXPECT_FALSE(body.isSleepEnabled());

    body.setType(Body::Type::Kinematic);
    const float start = body.getPosition().y;
    body.setLinearVelocity({0.0F, 1.0F, 0.0F});
    run(world, 60);
    EXPECT_NEAR(body.getPosition().y, start + 1.0F, 1.0e-3F);
}

TEST_F(PhysicsBody3DTest, CarriesWhatStandsOnAKinematicBodyThatMovesToItsTargets) {
    World world({}, backend);
    Body platform = world.createBody({.type = Body::Type::Kinematic, .shape = make(Shape::Box{.size = {4.0F, 0.5F, 4.0F}}), .friction = 1.0F});
    const Body rider = world.createBody({.shape = crate, .position = {0.0F, 0.75F, 0.0F}, .friction = 1.0F});
    run(world, 30);
    for (int step = 1; step <= 60; ++step) {
        platform.moveTo({.position = {static_cast<float>(step) * 0.02F, 0.0F, 0.0F}}, kStep);
        world.step(kStep);
    }

    EXPECT_NEAR(platform.getPosition().x, 1.2F, 1.0e-3F);
    EXPECT_NEAR(rider.getPosition().x, 1.2F, 0.1F);
    EXPECT_EQ(refusal<std::invalid_argument>([&] { platform.moveTo({}, 0.0F); }), "A 3D body moves to a transform over a time above zero seconds.");
}

TEST_F(PhysicsBody3DTest, TeleportsAndReadsAndWritesManyTransformsAtOnce) {
    World world({.gravity = {}}, backend);
    std::vector<Body> bodies;
    for (int index = 0; index < 3; ++index) {
        bodies.push_back(world.createBody({.shape = ball, .position = {static_cast<float>(index), 0.0F, 0.0F}}));
    }

    bodies[1].setTransform({.position = {0.0F, 9.0F, 0.0F}, .rotation = math::Quat::fromAxisAngle({0.0F, 1.0F, 0.0F}, 1.0F)});
    std::array<float, 21> values{};
    world.readTransforms(bodies, values);
    EXPECT_FLOAT_EQ(values[0], 0.0F);
    EXPECT_FLOAT_EQ(values[8], 9.0F);
    EXPECT_NEAR(values[11], std::sin(0.5F), 1.0e-5F);
    EXPECT_NEAR(values[13], std::cos(0.5F), 1.0e-5F);
    EXPECT_FLOAT_EQ(values[14], 2.0F);

    values[15] = 7.0F;
    values[20] = 2.0F;
    world.writeTransforms(bodies, values);
    EXPECT_FLOAT_EQ(bodies[2].getPosition().y, 7.0F);
    EXPECT_NEAR(bodies[2].getRotation().w, 1.0F, 1.0e-6F);

    std::array<float, 20> shortValues{};
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.readTransforms(bodies, shortValues); }), "Body transforms take seven floats for each body.");
    World other({}, backend);
    const std::vector<Body> foreign{other.createBody({.shape = ball})};
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.readTransforms(foreign, values); }), "Body transforms need live bodies of this world.");
    EXPECT_EQ(refusal<std::logic_error>([&] { world.readTransforms(bodies, values, 0.5F); }), "Only a 3D physics world made with \"interpolate\" blends the transforms of its bodies.");
}

TEST_F(PhysicsBody3DTest, BlendsTheTransformsOfMovingBodiesBetweenTheirLastTwoSteps) {
    World world({.gravity = {}, .interpolate = true}, backend);
    Body moving = world.createBody({.shape = ball, .linearVelocity = {6.0F, 0.0F, 0.0F}, .linearDamping = 0.0F});
    const Body resting = world.createBody({.type = Body::Type::Static, .shape = ball, .position = {0.0F, 5.0F, 0.0F}});
    run(world, 2);
    const float before = 6.0F * kStep;
    const float after = 6.0F * kStep * 2.0F;
    EXPECT_NEAR(world.getInterpolatedTransform(moving, 0.0F).position.x, before, 1.0e-4F);
    EXPECT_NEAR(world.getInterpolatedTransform(moving, 0.5F).position.x, (before + after) * 0.5F, 1.0e-4F);
    EXPECT_NEAR(world.getInterpolatedTransform(resting, 0.5F).position.y, 5.0F, 1.0e-6F);

    moving.setPosition({20.0F, 0.0F, 0.0F});
    EXPECT_NEAR(world.getInterpolatedTransform(moving, 0.0F).position.x, 20.0F, 1.0e-6F);
}

TEST_F(PhysicsBody3DTest, RunsItsStepHooksAroundEachStep) {
    World world({.gravity = {}}, backend);
    Body body = world.createBody({.shape = ball, .linearDamping = 0.0F});
    std::vector<std::string> calls;
    // clang-format off
    const std::uint64_t before = world.addStepHook(World::StepPhase::Before, [&](float) {
        calls.emplace_back("before");
        body.applyForce({body.getMass(), 0.0F, 0.0F});
    });
    // clang-format on
    world.addStepHook(World::StepPhase::After, [&](float seconds) { calls.push_back(seconds > 0.0F ? "after" : "none"); });
    world.step(kStep);
    EXPECT_EQ(calls, (std::vector<std::string>{"before", "after"}));
    EXPECT_NEAR(body.getLinearVelocity().x, kStep, 1.0e-5F);

    world.removeStepHook(before);
    world.addStepHook(World::StepPhase::Before, [&](float) { world.destroy(); });
    world.step(kStep);
    EXPECT_TRUE(world.isDestroyed());
    EXPECT_EQ(calls.size(), 2U);
}

TEST_F(PhysicsBody3DTest, TurnsItsHandlesInvalidOnceTheBodyOrItsWorldGoes) {
    auto world = std::make_unique<World>(WorldSettings{}, backend);
    Body body = world->createBody({.shape = ball});
    const Body kept = world->createBody({.shape = ball});
    EXPECT_TRUE(body.isValid());
    body.destroy();
    EXPECT_FALSE(body.isValid());
    EXPECT_EQ(refusal<std::logic_error>([&] { static_cast<void>(body.getPosition()); }), "The 3D physics body was destroyed.");

    const Body reused = world->createBody({.shape = ball});
    EXPECT_EQ(reused.getId() & 0x7FFFFFU, body.getId() & 0x7FFFFFU);
    EXPECT_FALSE(body.isValid());
    EXPECT_TRUE(reused.isValid());

    world->destroy();
    EXPECT_FALSE(kept.isValid());
    EXPECT_EQ(refusal<std::logic_error>([&] { static_cast<void>(kept.getPosition()); }), "The 3D physics world of the body was destroyed.");

    // A world made after the destroyed one may take its slot, and the handles of the old world stay invalid.
    world.reset();
    World next({}, backend);
    const Body fresh = next.createBody({.shape = ball});
    EXPECT_FALSE(kept.isValid());
    EXPECT_TRUE(fresh.isValid());
}

TEST_F(PhysicsBody3DTest, GivesEachBodyItsOwnCopyOfAMutableCompound) {
    World world({.gravity = {}}, backend);
    const std::shared_ptr<const Shape> ship = make(Shape::MutableCompound{.children = {{.shape = crate}}});
    Body first = world.createBody({.shape = ship});
    const Body second = world.createBody({.shape = ship, .position = {10.0F, 0.0F, 0.0F}});
    const float single = first.getMass();

    const std::size_t index = first.addChild({.shape = crate, .position = {2.0F, 0.0F, 0.0F}});
    EXPECT_EQ(index, 1U);
    EXPECT_EQ(first.getChildCount(), 2U);
    EXPECT_EQ(second.getChildCount(), 1U);
    EXPECT_NEAR(first.getMass(), 2.0F * single, 1.0F);
    EXPECT_NEAR(first.getWorldCenter().x, 1.0F, 1.0e-4F);
    EXPECT_NEAR(first.getPosition().x, 0.0F, 1.0e-4F);
    EXPECT_NEAR(first.getBounds().max.x, 2.5F, 0.01F);

    first.setChildTransform(1, {.position = {0.0F, 2.0F, 0.0F}});
    EXPECT_NEAR(first.getBounds().max.y, 2.5F, 0.01F);
    first.removeChild(0);
    EXPECT_EQ(first.getChildCount(), 1U);
    EXPECT_EQ(refusal<std::logic_error>([&] { first.removeChild(0); }), "A dynamic 3D body keeps at least one child, since a body without a volume has no mass. Make the body static or kinematic first, or add another child before you remove this one.");
    EXPECT_EQ(refusal<std::out_of_range>([&] { first.setChildTransform(4, {}); }), "The 3D body has no child at the index 4.");

    const Body plain = world.createBody({.shape = crate});
    EXPECT_EQ(refusal<std::logic_error>([&] { static_cast<void>(plain.getChildCount()); }), "Only a 3D body whose shape is a mutable compound has children of its own.");
}

TEST_F(PhysicsBody3DTest, RefusesBodiesThatTheirShapeOrOptionsDoNotSuit) {
    World world({}, backend);
    const std::shared_ptr<const Shape> floor = make(Shape::Mesh{.positions = {-5.0F, 0.0F, -5.0F, 5.0F, 0.0F, -5.0F, 0.0F, 0.0F, 5.0F}});
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createBody({.shape = floor}); }), "A dynamic 3D body needs a shape with a volume, and a mesh has none. Make the body static or kinematic, or give it a convex hull or a compound of convex shapes.");
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createBody({}); }), "A 3D body needs a shape.");
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createBody({.shape = ball, .locks = {true, true, true, true, true, true}}); }), "A dynamic 3D body locked on every axis never moves. Make it static, or leave one axis free.");
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createBody({.type = Body::Type::Static, .shape = ball, .linearVelocity = {1.0F, 0.0F, 0.0F}}); }), "A static 3D body never moves, so it takes no velocity. Make it kinematic to move it.");
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createBody({.shape = ball, .linearDamping = -1.0F}); }), "The linear damping of a 3D body must be a finite number of zero or more.");
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createBody({.shape = ball, .rotation = {0.0F, 0.0F, 0.0F, 0.0F}}); }), "The rotation of a 3D body must be a rotation, a quaternion of length one.");
    EXPECT_EQ(refusal<std::invalid_argument>([&] { world.createBody({.shape = ball, .mass = 0.0F}); }), "The mass of a 3D body must be a finite number above zero.");

    Body ground = world.createBody({.type = Body::Type::Static, .shape = floor});
    EXPECT_EQ(ground.getMass(), 0.0F);
    EXPECT_EQ(refusal<std::invalid_argument>([&] { ground.setType(Body::Type::Dynamic); }), "A dynamic 3D body needs a shape with a volume, and a mesh has none. Make the body static or kinematic, or give it a convex hull or a compound of convex shapes.");
    EXPECT_EQ(refusal<std::logic_error>([&] { ground.applyForce({1.0F, 0.0F, 0.0F}); }), "Only a dynamic 3D body takes forces.");

    World small({.maxBodies = 1}, backend);
    small.createBody({.shape = ball});
    EXPECT_EQ(refusal<std::length_error>([&] { small.createBody({.shape = ball}); }), "The 3D physics world holds \"maxBodies\" bodies already. Raise \"maxBodies\" in the \"physics3d\" section of \"haylen.json\" or in the options of the world, or destroy bodies it no longer needs.");
}

// A pile of bodies that falls, hits the floor and topples rebuilds the trees of the broad phase in most of its steps, which reuse the memory the world reserved.
TEST_F(PhysicsBody3DTest, StepsAPileOfMovingBodiesWithHooksAndInterpolationWithoutAllocating) {
    World world({.sleepEnabled = false, .interpolate = true}, backend);
    addFloor(world);
    std::vector<Body> bodies;
    for (int index = 0; index < 40; ++index) {
        const math::Vec3 position{static_cast<float>(index % 5) * 1.2F - 2.4F, 1.0F + static_cast<float>(index / 5) * 1.5F, static_cast<float>(index % 3) * 0.3F};
        bodies.push_back(world.createBody({.shape = index % 2 == 0 ? ball : crate, .position = position, .angularVelocity = {0.0F, 0.0F, 2.0F}}));
    }

    float pushed = 0.0F;
    world.addStepHook(World::StepPhase::Before, [&](float seconds) { pushed += seconds; });
    std::vector<float> values(bodies.size() * World::kTransformFloats);
    world.step(kStep);
    world.readTransforms(bodies, values);

    const test::AllocationTracker tracker;
    for (int step = 0; step < 180; ++step) {
        world.step(kStep);
        world.readTransforms(bodies, values, 0.5F);
        world.readTransforms(bodies, values);
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_GT(pushed, 0.0F);
    EXPECT_GT(world.getStats().activeBodies, 0);
}

} // namespace haylen::physics3d
