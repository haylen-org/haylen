#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
#include "support/AllocationTracker.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::physics2d {

class PhysicsWorldTest : public ::testing::Test {
  protected:
    static void simulate(World& world, float seconds) {
        for (float time = 0.0F; time < seconds; time += 1.0F / 60.0F) {
            world.step(1.0F / 60.0F);
        }
    }
};

TEST_F(PhysicsWorldTest, DropsBodiesOntoGroundAndReportsContacts) {
    World world({.gravity = {0.0F, 980.0F}, .pixelsPerMeter = 64.0F});
    Body ground = world.createBody({.type = Body::Type::Static, .position = {0.0F, 400.0F}});
    ground.addBox({1000.0F, 20.0F});
    Body crate = world.createBody({.position = {0.0F, 0.0F}});
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
    EXPECT_EQ(crate.getType(), Body::Type::Dynamic);
    EXPECT_EQ(ground.getShapes().size(), 1U);
    EXPECT_EQ(world.getGravity(), haylen::math::Vec2(0.0F, 980.0F));

    crate.destroy();
    world.step(1.0F / 60.0F);
    EXPECT_FALSE(crate.isValid());
    EXPECT_EQ(world.getBodyCount(), 1U);
    EXPECT_THROW((void)crate.getPosition(), std::logic_error);
    crate.destroy();
}

TEST_F(PhysicsWorldTest, MovesAndConfiguresBodies) {
    World world({.gravity = {}});
    Body body = world.createBody({.position = {10.0F, 20.0F}, .rotation = 0.5F, .velocity = {64.0F, 0.0F}, .linearDamping = 0.5F, .angularDamping = 0.25F, .gravityScale = 2.0F, .locks = {.rotation = true}, .bullet = true});
    body.addCircle(16.0F);
    EXPECT_EQ(body.getPosition(), haylen::math::Vec2(10.0F, 20.0F));
    EXPECT_NEAR(body.getRotation(), 0.5F, 0.002F);
    EXPECT_EQ(body.getVelocity(), haylen::math::Vec2(64.0F, 0.0F));
    EXPECT_FLOAT_EQ(body.getLinearDamping(), 0.5F);
    EXPECT_FLOAT_EQ(body.getAngularDamping(), 0.25F);
    EXPECT_FLOAT_EQ(body.getGravityScale(), 2.0F);
    EXPECT_TRUE(body.getMotionLocks().rotation);
    EXPECT_TRUE(body.isBullet());

    body.setLinearDamping(0.0F);
    body.setAngularDamping(0.0F);
    body.setGravityScale(1.0F);
    body.setMotionLocks({});
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

    body.setType(Body::Type::Kinematic);
    EXPECT_EQ(body.getType(), Body::Type::Kinematic);
    body.setEnabled(false);
    EXPECT_FALSE(body.isEnabled());
    body.setEnabled(true);
    body.setAwake(false);
    EXPECT_FALSE(body.isAwake());
    world.setGravity({0.0F, 10.0F});
    EXPECT_EQ(world.getGravity(), haylen::math::Vec2(0.0F, 10.0F));
}

TEST_F(PhysicsWorldTest, UsesWorldUnitsForTorquesAndMotors) {
    // A disc spins and a slider moves by the same amounts at every scale, because inertia, torque and force all use world units.
    for (const float scale : {32.0F, 64.0F}) {
        World world({.gravity = {}, .pixelsPerMeter = scale});
        Body disc = world.createBody();
        disc.addCircle(32.0F);
        const float discInertia = 0.5F * disc.getMass() * 32.0F * 32.0F;
        disc.applyAngularImpulse(discInertia * 2.0F);
        EXPECT_NEAR(disc.getAngularVelocity(), 2.0F, 0.01F);
        disc.applyTorque(discInertia * 4.0F);

        const Body anchor = world.createBody({.type = Body::Type::Static});
        Body wheel = world.createBody({.position = {200.0F, 0.0F}});
        wheel.addCircle(32.0F);
        const float wheelInertia = 0.5F * wheel.getMass() * 32.0F * 32.0F;
        world.createJoint(Joint::Type::Revolute, anchor, wheel, {.anchorA = {200.0F, 0.0F}, .enableMotor = true, .motorSpeed = 100.0F, .maxMotorTorque = wheelInertia * 4.0F});
        Body slider = world.createBody({.position = {0.0F, 200.0F}});
        slider.addBox({32.0F, 32.0F});
        world.createJoint(Joint::Type::Prismatic, anchor, slider, {.anchorA = {0.0F, 200.0F}, .enableMotor = true, .motorSpeed = 10000.0F, .maxMotorForce = slider.getMass() * 60.0F});

        world.step(0.25F);
        EXPECT_NEAR(disc.getAngularVelocity(), 3.0F, 0.01F) << scale;
        EXPECT_NEAR(wheel.getAngularVelocity(), 1.0F, 0.02F) << scale;
        EXPECT_NEAR(slider.getVelocity().x, 15.0F, 0.3F) << scale;
    }
}

TEST_F(PhysicsWorldTest, SensesQueriesAndCastsRays) {
    World world({.gravity = {0.0F, 980.0F}});
    Body zone = world.createBody({.type = Body::Type::Static, .position = {0.0F, 200.0F}});
    const Shape sensor = zone.addBox({200.0F, 40.0F}, {.sensor = true});
    Body wall = world.createBody({.type = Body::Type::Static, .position = {300.0F, 0.0F}});
    const Shape wallShape = wall.addBox({20.0F, 400.0F}, {.filter = {.category = 2}});
    Body ball = world.createBody({.position = {0.0F, 0.0F}});
    ball.addCircle(8.0F);

    std::size_t sensed = 0;
    std::size_t left = 0;
    for (int step = 0; step < 120; ++step) {
        world.step(1.0F / 60.0F);

        for (const SensorEvent& event : world.getSensorBegins()) {
            EXPECT_EQ(event.sensor, sensor);
            EXPECT_EQ(event.visitor.getBody(), ball);
            ++sensed;
        }

        left += world.getSensorEnds().size();
    }

    EXPECT_EQ(sensed, 1U);
    EXPECT_EQ(left, 1U);
    EXPECT_TRUE(sensor.isSensor());

    const std::optional<RaycastHit> hit = world.raycast({0.0F, -100.0F}, {600.0F, -100.0F});
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

    Shape moved = wallShape;
    moved.setFilter({.category = 4, .mask = 1});
    EXPECT_EQ(moved.getFilter().category, 4U);
    EXPECT_EQ(moved.getFilter().mask, 1U);
}

TEST_F(PhysicsWorldTest, LocksBodiesAlongTheAxesOfTheWorld) {
    // A body locked along x falls straight down whatever pushes it sideways, one locked along y floats at its height under gravity, and one locked in rotation keeps its angle when it is spun.
    World world;
    Body lift = world.createBody({.velocity = {300.0F, 0.0F}, .locks = {.x = true}});
    lift.addBox({20.0F, 20.0F});
    Body sledge = world.createBody({.position = {100.0F, 0.0F}, .velocity = {300.0F, 0.0F}, .locks = {.y = true}});
    sledge.addBox({20.0F, 20.0F});
    Body turnstile = world.createBody({.position = {200.0F, -300.0F}, .locks = {.x = true, .y = true}});
    turnstile.addBox({80.0F, 10.0F});
    Body upright = world.createBody({.position = {300.0F, -600.0F}, .locks = {.rotation = true}});
    upright.addBox({20.0F, 40.0F});
    turnstile.applyAngularImpulse(turnstile.getInertia() * 3.0F);
    upright.applyAngularImpulse(upright.getInertia() * 3.0F + 1000.0F);
    simulate(world, 0.5F);
    EXPECT_NEAR(lift.getPosition().x, 0.0F, 0.01F);
    EXPECT_GT(lift.getPosition().y, 50.0F);
    EXPECT_NEAR(sledge.getPosition().y, 0.0F, 0.01F);
    EXPECT_GT(sledge.getPosition().x, 200.0F);
    EXPECT_NEAR(turnstile.getPosition().y, -300.0F, 0.01F);
    EXPECT_GT(std::abs(turnstile.getRotation()), 1.0F);
    EXPECT_NEAR(upright.getRotation(), 0.0F, 1e-4F);

    // Locks change while the body runs, so a sledge set free falls.
    EXPECT_TRUE(sledge.getMotionLocks().y);
    sledge.setMotionLocks({});
    EXPECT_FALSE(sledge.getMotionLocks().y);
    simulate(world, 0.5F);
    EXPECT_GT(sledge.getPosition().y, 50.0F);
}

TEST_F(PhysicsWorldTest, SweepsBodiesFromTheirContinuousThreshold) {
    // A thin sensor in the path of a fast body sees it cross, although the body stands on neither side of it at the end of any step.
    World world({.gravity = {}});
    Body gate = world.createBody({.type = Body::Type::Static, .position = {200.0F, 0.0F}});
    gate.addBox({4.0F, 200.0F}, {.sensor = true});
    Body shot = world.createBody({.velocity = {6000.0F, 0.0F}, .bullet = false, .continuousThreshold = 0.1F});
    shot.addCircle(4.0F);
    EXPECT_FLOAT_EQ(shot.getContinuousThreshold(), 0.1F);
    std::size_t seen = 0;
    for (int step = 0; step < 10; ++step) {
        world.step(1.0F / 60.0F);
        seen += world.getSensorBegins().size();
    }

    EXPECT_EQ(seen, 1U);
    EXPECT_GT(shot.getPosition().x, 400.0F);

    shot.setContinuousThreshold(0.5F);
    EXPECT_FLOAT_EQ(shot.getContinuousThreshold(), 0.5F);
    EXPECT_THROW(shot.setContinuousThreshold(-1.0F), std::invalid_argument);
    EXPECT_THROW((void)world.createBody({.continuousThreshold = std::nanf("")}), std::invalid_argument);
}

TEST_F(PhysicsWorldTest, MergesChainPointsCloserThanTheLinearSlop) {
    // Points a tenth of a world unit apart merge, and so does a loop end that closes on its start, so the outline keeps one segment per side.
    World world;
    Body ground = world.createBody({.type = Body::Type::Static});
    const std::vector<haylen::math::Vec2> square{{0.0F, 0.0F}, {100.0F, 0.0F}, {100.1F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}, {0.0F, 0.1F}};
    EXPECT_EQ(ground.addChain(square, true).size(), 4U);
    const std::vector<haylen::math::Vec2> speck{{0.0F, 0.0F}, {0.1F, 0.0F}, {0.1F, 0.1F}, {0.0F, 0.1F}};
    EXPECT_THROW((void)ground.addChain(speck, true), std::invalid_argument);
}

TEST_F(PhysicsWorldTest, FindsPointsOnTheDiagonalsOfAPolygon) {
    // The middle of a small box away from the origin of its body lies on both diagonals of the box, where a search for the distance to the box loses its direction.
    World world;
    Body holder = world.createBody({.type = Body::Type::Static, .position = {100.0F, 100.0F}});
    holder.addBox({8.0F, 8.0F}, {.sensor = true, .offset = {-4.0F, 4.0F}});
    EXPECT_EQ(world.queryPoint({96.0F, 104.0F}).size(), 1U);
    EXPECT_EQ(world.queryPoint({98.0F, 102.0F}).size(), 1U);
    EXPECT_TRUE(world.queryPoint({91.0F, 104.0F}).empty());
}

TEST_F(PhysicsWorldTest, BuildsPolygonsChainsAndCapsules) {
    World world;
    Body body = world.createBody({.type = Body::Type::Static});

    const std::vector<haylen::math::Vec2> square{{0.0F, 0.0F}, {32.0F, 0.0F}, {32.0F, 32.0F}, {0.0F, 32.0F}};
    EXPECT_EQ(body.addPolygon(square).size(), 1U);
    const std::vector<haylen::math::Vec2> dent{{0.0F, 0.0F}, {40.0F, 0.0F}, {20.0F, 10.0F}, {40.0F, 40.0F}, {0.0F, 40.0F}};
    EXPECT_EQ(body.addPolygon(dent, {.offset = {100.0F, 0.0F}, .rotation = 0.1F}).size(), 2U);

    const std::vector<haylen::math::Vec2> outline{{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}};
    const std::vector<Shape> chain = body.addChain(outline, true);
    EXPECT_EQ(chain.size(), 4U);
    EXPECT_EQ(body.addChain(std::vector<haylen::math::Vec2>{{0.0F, 200.0F}, {100.0F, 200.0F}}, false).size(), 1U);

    Shape capsule = body.addCapsule({0.0F, 0.0F}, {0.0F, 50.0F}, 10.0F);
    Shape segment = body.addSegment({0.0F, 0.0F}, {50.0F, 0.0F});
    EXPECT_TRUE(capsule.isValid());
    capsule.destroy();
    EXPECT_FALSE(capsule.isValid());
    EXPECT_THROW((void)capsule.getBounds(), std::logic_error);
    segment.destroy();

    EXPECT_THROW(body.addBox({0.0F, 10.0F}), std::invalid_argument);
    EXPECT_THROW(body.addCircle(0.0F), std::invalid_argument);
    EXPECT_THROW(body.addCapsule({}, {1.0F, 0.0F}, 0.0F), std::invalid_argument);
    EXPECT_THROW(body.addPolygon(std::vector<haylen::math::Vec2>{{0.0F, 0.0F}, {1.0F, 0.0F}}), std::invalid_argument);
    EXPECT_THROW(body.addPolygon(std::vector<haylen::math::Vec2>{{0.0F, 0.0F}, {1.0F, 0.0F}, {2.0F, 0.0F}}), std::invalid_argument);
    EXPECT_THROW(body.addChain(std::vector<haylen::math::Vec2>{{0.0F, 0.0F}, {1.0F, 0.0F}, {2.0F, 0.0F}}, true), std::invalid_argument);
    EXPECT_THROW(body.addChain(std::vector<haylen::math::Vec2>{{0.0F, 0.0F}}, false), std::invalid_argument);
    EXPECT_THROW(World({.pixelsPerMeter = 0.0F}), std::invalid_argument);
}

TEST_F(PhysicsWorldTest, RejectsWhatBox2DCannotHoldBeforeCreatingIt) {
    World world;
    Body body = world.createBody();
    const std::vector<haylen::math::Vec2> outline{{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}};

    EXPECT_THROW(body.addSegment({10.0F, 0.0F}, {10.1F, 0.0F}), std::invalid_argument);
    EXPECT_THROW(body.addBox({10.0F, 10.0F}, {.density = -1.0F}), std::invalid_argument);
    EXPECT_THROW(body.addCircle(5.0F, {.friction = std::numeric_limits<float>::quiet_NaN()}), std::invalid_argument);
    EXPECT_THROW(body.addBox({10.0F, 10.0F}, {.oneWay = haylen::math::Vec2{}}), std::invalid_argument);
    EXPECT_THROW(body.addChain(outline, true, {.restitution = -0.5F}), std::invalid_argument);
    EXPECT_THROW(body.addChain(outline, true, {.oneWay = haylen::math::Vec2{}}), std::invalid_argument);
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
    std::vector<std::unique_ptr<World>> worlds;
    EXPECT_THROW(for (int index = 0; index < 1000; ++index) { worlds.push_back(std::make_unique<World>()); }, std::runtime_error);

    // A world that goes makes room for another one.
    worlds.pop_back();
    EXPECT_NO_THROW(worlds.push_back(std::make_unique<World>()));
}

// A new world reuses the slot of a destroyed one and hands out the same body, shape and joint ids, so a handle checks the generation of its world as well.
TEST_F(PhysicsWorldTest, DetectsHandlesOfADestroyedWorldWhoseSlotANewWorldReuses) {
    Body staleBody;
    Shape staleShape;
    Joint staleJoint;
    {
        World old;
        staleBody = old.createBody();
        staleShape = staleBody.addBox({10.0F, 10.0F});
        staleJoint = old.createJoint(Joint::Type::Weld, staleBody, old.createBody({.position = {20.0F, 0.0F}}));
    }

    World reused;
    Body body = reused.createBody();
    const Shape shape = body.addBox({10.0F, 10.0F});
    const Joint joint = reused.createJoint(Joint::Type::Weld, body, reused.createBody({.position = {20.0F, 0.0F}}));
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

// A world destroyed before the object goes ends its bodies, shapes, joints and hooks at once, and every later use fails with a sentence instead of reaching Box2D.
TEST_F(PhysicsWorldTest, DestroysTheWorldBeforeItsObjectGoes) {
    World world;
    Body body = world.createBody();
    const Shape shape = body.addBox({10.0F, 10.0F});
    const Joint joint = world.createJoint(Joint::Type::Weld, body, world.createBody({.position = {20.0F, 0.0F}}));
    int hookRuns = 0;
    world.addStepHook(World::StepPhase::Before, [&hookRuns](float) { ++hookRuns; });

    world.destroy();
    world.destroy();
    EXPECT_TRUE(world.isDestroyed());
    EXPECT_FALSE(body.isValid() || shape.isValid() || joint.isValid());
    EXPECT_EQ(hookRuns, 0);

    for (const auto& [use, message] : std::array<std::pair<std::function<void()>, std::string>, 6>{{
             {[&world] { world.step(1.0F / 60.0F); }, "The physics world was destroyed."},
             {[&world] { (void)world.createBody(); }, "The physics world was destroyed."},
             {[&world] { (void)world.queryPoint({0.0F, 0.0F}); }, "The physics world was destroyed."},
             {[&world] { (void)world.raycast({0.0F, 0.0F}, {10.0F, 0.0F}); }, "The physics world was destroyed."},
             {[&world] { world.addStepHook(World::StepPhase::After, [](float) {}); }, "The physics world was destroyed."},
             {[&body] { (void)body.getPosition(); }, "The physics world of the body was destroyed."},
         }}) {
        try {
            use();
            ADD_FAILURE() << message;
        } catch (const std::logic_error& error) {
            EXPECT_EQ(std::string(error.what()), message);
        }
    }

    // Handles of the destroyed world destroy nothing, so the owners of bodies, such as vehicles and terrains, go quietly after it.
    body.destroy();
    EXPECT_FALSE(body.isValid());
}

// A hook may add and remove hooks while the step runs them: each phase runs the hooks it had when it began, except those removed since, and a step that repeats allocates nothing.
TEST_F(PhysicsWorldTest, RunsHooksThatAddAndRemoveHooks) {
    World world;
    std::vector<std::string> runs;
    std::uint64_t second = 0;
    std::uint64_t first = 0;
    // clang-format off
    first = world.addStepHook(World::StepPhase::Before, [&](float) {
        runs.emplace_back("first");
        world.removeStepHook(first);
        world.removeStepHook(second);
        for (int index = 0; index < 64; ++index) {
            world.addStepHook(World::StepPhase::Before, [&runs](float) { runs.emplace_back("added"); });
        }
    });
    // clang-format on
    second = world.addStepHook(World::StepPhase::Before, [&runs](float) { runs.emplace_back("second"); });
    const std::uint64_t third = world.addStepHook(World::StepPhase::Before, [&runs](float) { runs.emplace_back("third"); });

    world.step(1.0F / 60.0F);
    EXPECT_EQ(runs, (std::vector<std::string>{"first", "third"}));

    runs.clear();
    world.step(1.0F / 60.0F);
    EXPECT_EQ(runs.size(), 65U);
    EXPECT_EQ(runs.front(), "third");

    world.removeStepHook(third);
    runs.reserve(256);
    world.step(1.0F / 60.0F);
    const test::AllocationTracker tracker;
    world.step(1.0F / 60.0F);
    EXPECT_EQ(tracker.getTotal(), 0U);

    // A hook that destroys the world ends the step.
    world.addStepHook(World::StepPhase::Before, [&world](float) { world.destroy(); });
    world.step(1.0F / 60.0F);
    EXPECT_TRUE(world.isDestroyed());
}

TEST_F(PhysicsWorldTest, ReportsTheGeometryAndOutlinesOfShapes) {
    World world;
    Body body = world.createBody({.type = Body::Type::Static, .position = {100.0F, 50.0F}, .rotation = std::numbers::pi_v<float> * 0.5F});
    // clang-format off
    const auto expectInWorld = [](haylen::math::Vec2 local, haylen::math::Vec2 placed) {
        EXPECT_NEAR(placed.x, 100.0F - local.y, 1e-3F);
        EXPECT_NEAR(placed.y, 50.0F + local.x, 1e-3F);
    };
    // clang-format on

    // A quarter turn maps the body point (x, y) to (100 - y, 50 + x) in the world.
    const Shape box = body.addBox({40.0F, 20.0F}, {.offset = {10.0F, 0.0F}});
    const std::vector<haylen::math::Vec2> corners = box.getPoints();
    ASSERT_EQ(corners.size(), 4U);
    EXPECT_EQ(box.getKind(), Shape::Kind::Polygon);
    EXPECT_EQ(box.getRadius(), 0.0F);

    for (std::size_t index = 0; index < corners.size(); ++index) {
        EXPECT_NEAR(std::abs(corners[index].x - 10.0F), 20.0F, 1e-3F);
        EXPECT_NEAR(std::abs(corners[index].y), 10.0F, 1e-3F);
        expectInWorld(corners[index], box.getWorldPoints()[index]);
    }

    EXPECT_TRUE(box.getOutline().closed);
    EXPECT_EQ(box.getOutline().points.size(), 4U);

    const Shape circle = body.addCircle(12.0F, {.offset = {0.0F, 30.0F}});
    EXPECT_EQ(circle.getKind(), Shape::Kind::Circle);
    EXPECT_EQ(circle.getPoints(), std::vector<haylen::math::Vec2>({{0.0F, 30.0F}}));
    EXPECT_FLOAT_EQ(circle.getRadius(), 12.0F);
    const Shape::Outline round = circle.getOutline();
    EXPECT_TRUE(round.closed);
    EXPECT_EQ(round.points.size(), 16U);

    for (const haylen::math::Vec2 point : round.points) {
        EXPECT_NEAR(haylen::math::Vec2::distance(point, {70.0F, 50.0F}), 12.0F, 1e-3F);
    }

    const Shape capsule = body.addCapsule({0.0F, -10.0F}, {0.0F, 10.0F}, 6.0F);
    EXPECT_EQ(capsule.getKind(), Shape::Kind::Capsule);
    EXPECT_FLOAT_EQ(capsule.getRadius(), 6.0F);
    expectInWorld({0.0F, 10.0F}, capsule.getWorldPoints()[1]);

    // Every point of the outline of a capsule lies its radius away from the segment between its centers, which runs from (90, 50) to (110, 50) in the world.
    const Shape::Outline pill = capsule.getOutline();
    EXPECT_TRUE(pill.closed);
    EXPECT_EQ(pill.points.size(), 18U);

    for (const haylen::math::Vec2 point : pill.points) {
        EXPECT_NEAR(haylen::math::Vec2::distance(point, {std::clamp(point.x, 90.0F, 110.0F), 50.0F}), 6.0F, 1e-3F);
    }

    const Shape segment = body.addSegment({-50.0F, 0.0F}, {50.0F, 0.0F});
    EXPECT_EQ(segment.getKind(), Shape::Kind::Segment);
    EXPECT_FALSE(segment.getOutline().closed);
    expectInWorld({50.0F, 0.0F}, segment.getOutline().points[1]);
    EXPECT_EQ(body.getOutlines().size(), 4U);

    // Chains join their segments into one outline, closed for a loop and from the first to the last point for an open chain.
    Body ground = world.createBody({.type = Body::Type::Static});
    const std::vector<Shape> loop = ground.addChain(std::vector<haylen::math::Vec2>{{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}}, true);
    (void)ground.addChain(std::vector<haylen::math::Vec2>{{0.0F, 200.0F}, {50.0F, 200.0F}, {100.0F, 210.0F}, {150.0F, 200.0F}, {200.0F, 200.0F}}, false);
    EXPECT_EQ(loop.front().getKind(), Shape::Kind::ChainSegment);
    EXPECT_EQ(loop.front().getPoints().size(), 2U);
    const std::vector<Shape::Outline> outlines = ground.getOutlines();
    ASSERT_EQ(outlines.size(), 2U);
    const Shape::Outline& closed = outlines[0].closed ? outlines[0] : outlines[1];
    const Shape::Outline& open = outlines[0].closed ? outlines[1] : outlines[0];
    EXPECT_EQ(closed.points, std::vector<haylen::math::Vec2>({{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}}));
    EXPECT_FALSE(open.closed);
    EXPECT_EQ(open.points, std::vector<haylen::math::Vec2>({{0.0F, 200.0F}, {50.0F, 200.0F}, {100.0F, 210.0F}, {150.0F, 200.0F}, {200.0F, 200.0F}}));

    EXPECT_EQ(Shape::kindName(Shape::Kind::ChainSegment), "chainSegment");
    EXPECT_EQ(Shape::kindFromName("capsule"), Shape::Kind::Capsule);
    EXPECT_FALSE(Shape::kindFromName("triangle").has_value());
}

TEST_F(PhysicsWorldTest, ConnectsBodiesWithJoints) {
    World world({.gravity = {0.0F, 980.0F}});
    Body anchor = world.createBody({.type = Body::Type::Static, .position = {0.0F, 0.0F}});
    anchor.addCircle(4.0F);

    std::vector<Joint> joints;
    std::vector<Body> bobs;
    for (const Joint::Type type : {Joint::Type::Distance, Joint::Type::Revolute, Joint::Type::Prismatic, Joint::Type::Weld, Joint::Type::Wheel, Joint::Type::Mouse, Joint::Type::Motor}) {
        Body bob = world.createBody({.position = {100.0F, 0.0F}});
        bob.addCircle(8.0F, {.filter = {.group = -1}});
        bobs.push_back(bob);
        const bool limited = type != Joint::Type::Distance && type != Joint::Type::Revolute;
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
    EXPECT_THROW(joints[3].setMotorSpeed(1.0F), std::logic_error);
    EXPECT_THROW((void)joints[3].getMotorSpeed(), std::logic_error);
    EXPECT_THROW(joints[0].setTarget({}), std::logic_error);
    EXPECT_THROW((void)joints[1].getTarget(), std::logic_error);

    joints[0].destroy();
    EXPECT_FALSE(joints[0].isValid());
    EXPECT_THROW(joints[0].setMotorSpeed(1.0F), std::logic_error);
    joints[0].destroy();

    // A filter joint lets a crate fall through the floor it would otherwise land on.
    Body floor = world.createBody({.type = Body::Type::Static, .position = {500.0F, 200.0F}});
    floor.addBox({200.0F, 20.0F});
    Body ghost = world.createBody({.position = {500.0F, 100.0F}});
    ghost.addBox({20.0F, 20.0F});
    Body solid = world.createBody({.position = {560.0F, 100.0F}});
    solid.addBox({20.0F, 20.0F});
    EXPECT_TRUE(world.createJoint(Joint::Type::Filter, floor, ghost).isValid());
    simulate(world, 1.0F);
    EXPECT_GT(ghost.getPosition().y, 300.0F);
    EXPECT_LT(solid.getPosition().y, 200.0F);
    EXPECT_EQ(Joint::typeFromName("filter"), Joint::Type::Filter);

    World other;
    const Body stranger = other.createBody();
    EXPECT_THROW(world.createJoint(Joint::Type::Weld, anchor, stranger), std::invalid_argument);

    // Box2D cannot hold a distance joint between anchors in one place, revolute limits near a half turn or limits in the wrong order.
    EXPECT_THROW(world.createJoint(Joint::Type::Distance, anchor, ghost), std::invalid_argument);
    EXPECT_TRUE(world.createJoint(Joint::Type::Distance, anchor, ghost, {.length = 1.0F}).isValid());
    EXPECT_THROW(world.createJoint(Joint::Type::Revolute, anchor, ghost, {.lower = -3.13F, .upper = 0.0F}), std::invalid_argument);
    EXPECT_THROW(world.createJoint(Joint::Type::Revolute, anchor, ghost, {.lower = 0.5F, .upper = -0.5F}), std::invalid_argument);
    EXPECT_THROW(world.createJoint(Joint::Type::Prismatic, anchor, ghost, {.lower = 10.0F, .upper = -10.0F}), std::invalid_argument);
    EXPECT_THROW(world.createJoint(Joint::Type::Wheel, anchor, ghost, {.lower = 10.0F, .upper = -10.0F}), std::invalid_argument);
    EXPECT_THROW(world.createJoint(Joint::Type::Prismatic, anchor, ghost, {.axis = {}}), std::invalid_argument);
    EXPECT_TRUE(world.createJoint(Joint::Type::Revolute, anchor, ghost, {.lower = -3.1F, .upper = 3.1F}).isValid());
    EXPECT_EQ(Joint::typeFromName("wheel"), Joint::Type::Wheel);
    EXPECT_EQ(Joint::typeName(Joint::Type::Motor), "motor");
    EXPECT_FALSE(Joint::typeFromName("rope").has_value());
    EXPECT_EQ(Body::typeFromName("kinematic"), Body::Type::Kinematic);
    EXPECT_EQ(Body::typeName(Body::Type::Static), "static");
    EXPECT_FALSE(Body::typeFromName("ghost").has_value());
}

TEST_F(PhysicsWorldTest, MeasuresJointAnglesFromThePoseAtCreation) {
    // A tight revolute limit and a prismatic joint both keep a body turned the way it was when they joined it.
    World world({.gravity = {}});
    const Body anchor = world.createBody({.type = Body::Type::Static});
    Body hinged = world.createBody({.position = {100.0F, 0.0F}, .rotation = 1.0F, .angularVelocity = 0.5F});
    hinged.addBox({40.0F, 10.0F});
    Body slider = world.createBody({.position = {0.0F, 100.0F}, .rotation = 0.8F, .velocity = {20.0F, 0.0F}});
    slider.addBox({20.0F, 20.0F});
    world.createJoint(Joint::Type::Revolute, anchor, hinged, {.anchorA = {100.0F, 0.0F}, .enableLimit = true, .lower = -0.1F, .upper = 0.1F});
    world.createJoint(Joint::Type::Prismatic, anchor, slider, {.anchorA = {0.0F, 100.0F}});

    simulate(world, 1.0F);
    EXPECT_NEAR(hinged.getRotation(), 1.1F, 0.02F);
    EXPECT_NEAR(slider.getRotation(), 0.8F, 0.01F);
    EXPECT_GT(slider.getPosition().x, 10.0F);
}

TEST_F(PhysicsWorldTest, DrawsDebugShapes) {
    test::EngineFixture fixture;
    World world;
    Body body = world.createBody({.type = Body::Type::Static});
    body.addBox({20.0F, 20.0F});
    body.addCircle(5.0F, {.offset = {30.0F, 0.0F}});
    body.addCapsule({0.0F, 40.0F}, {20.0F, 40.0F}, 4.0F);
    body.addSegment({0.0F, 60.0F}, {20.0F, 60.0F});
    Body dynamic = world.createBody({.position = {100.0F, 0.0F}});
    dynamic.addCircle(5.0F);
    world.createJoint(Joint::Type::Revolute, body, dynamic, {.anchorA = {100.0F, 0.0F}});

    // clang-format off
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([&world](haylen::core::Engine& engine) {
        engine.getRenderer2D().beginWorld(haylen::graphics2d::Camera{});
        world.debugDraw(engine.getRenderer2D(), {.layer = 10});
    }));
    // clang-format on
    fixture.frames(1);
    const haylen::graphics2d::Renderer::Stats& stats = fixture.engine().getRenderer2D().getStats();
    EXPECT_GT(stats.instances, stats.sprites);
}

} // namespace haylen::physics2d
