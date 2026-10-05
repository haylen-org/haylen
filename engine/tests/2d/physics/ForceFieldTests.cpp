#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

#include "haylen/2d/physics/ForceField.hpp"
#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

class ForceFieldTest : public ::testing::Test {
  protected:
    void simulate(World& world, float seconds) {
        for (int step = 0; step < static_cast<int>(seconds * 60.0F); ++step) {
            world.step(1.0F / 60.0F);
        }
    }

    Body ball(World& world, math::Vec2 position, std::uint64_t category = 1) {
        Body body = world.createBody({.position = position});
        body.addCircle(10.0F, {.filter = {.category = category}});
        return body;
    }
};

TEST_F(ForceFieldTest, RadialFieldsPullWhatTheirMaskSees) {
    World space({.gravity = {}});
    ForceField magnet(space, {.kind = ForceField::Kind::Radial, .radius = 500.0F, .strength = 2000.0F, .linearDrag = 2.0F, .filter = {.mask = 1}});
    Body metal = ball(space, {300.0F, 0.0F});
    Body wood = ball(space, {-300.0F, 0.0F}, 2);
    simulate(space, 3.0F);
    EXPECT_LT(metal.getPosition().getLength(), 40.0F);
    EXPECT_NEAR(wood.getPosition().x, -300.0F, 0.01F);
    EXPECT_EQ(magnet.getBodyCount(), 1U);

    // A negative strength pushes away, and a disabled field does nothing.
    magnet.setStrength(-2000.0F);
    simulate(space, 1.0F);
    EXPECT_GT(metal.getPosition().getLength(), 100.0F);
    magnet.setEnabled(false);
    const math::Vec2 resting = metal.getPosition();
    metal.setVelocity({});
    simulate(space, 1.0F);
    EXPECT_EQ(metal.getPosition(), resting);
    EXPECT_THROW(ForceField(space, {}), std::invalid_argument);
    EXPECT_THROW(ForceField(space, {.radius = 10.0F, .falloff = ForceField::Falloff::InverseSquare}), std::invalid_argument);
}

TEST_F(ForceFieldTest, GroupsDecideLikeCollisions) {
    // A field leaves out the bodies of its own negative group even inside its mask and pulls those of its positive group even outside its mask, as shapes collide.
    for (const int group : {-1, 1}) {
        World space({.gravity = {}});
        ForceField magnet(space, {.kind = ForceField::Kind::Radial, .radius = 500.0F, .strength = 2000.0F, .filter = {.mask = group > 0 ? std::uint64_t{2} : ~std::uint64_t{0}, .group = group}});
        Body member = space.createBody({.position = {300.0F, 0.0F}});
        member.addCircle(10.0F, {.filter = {.group = group}});
        simulate(space, 1.0F);
        EXPECT_EQ(member.getPosition().x < 290.0F, group > 0);
    }
}

TEST_F(ForceFieldTest, CircularOrbitsKeepTheirRadius) {
    // A body launched at the circular speed of an inverse square field stays near its radius for a whole orbit, like a moon around a planet.
    World space({.gravity = {}, .subSteps = 8});
    const float surface = 100.0F;
    const float gravity = 800.0F;
    ForceField planet(space, {.kind = ForceField::Kind::Radial, .radius = 2000.0F, .strength = gravity, .falloff = ForceField::Falloff::InverseSquare, .minDistance = surface});
    const float orbit = 300.0F;
    const float speed = std::sqrt(gravity * surface * surface / orbit);
    Body moon = space.createBody({.position = {orbit, 0.0F}, .velocity = {0.0F, speed}});
    moon.addCircle(4.0F);
    const float period = 2.0F * 3.14159265F * orbit / speed;
    for (int step = 0; step < static_cast<int>(period * 60.0F); ++step) {
        space.step(1.0F / 60.0F);
        EXPECT_NEAR(moon.getPosition().getLength(), orbit, orbit * 0.05F);
    }
}

TEST_F(ForceFieldTest, WindPushesLightBodiesHarder) {
    // A force field pushes every body with the same force, so the lighter body moves further, and an acceleration field moves both alike.
    World space({.gravity = {}});
    ForceField wind(space, {.kind = ForceField::Kind::Directional, .size = {4000.0F, 400.0F}, .strength = 40.0F, .direction = {1.0F, 0.0F}, .acceleration = false});
    Body light = space.createBody({.position = {0.0F, -100.0F}});
    light.addBox({20.0F, 20.0F}, {.density = 0.5F});
    Body heavy = space.createBody({.position = {0.0F, 100.0F}});
    heavy.addBox({20.0F, 20.0F}, {.density = 2.0F});
    simulate(space, 1.0F);
    EXPECT_NEAR(light.getVelocity().x / heavy.getVelocity().x, 4.0F, 0.2F);

    ForceField vortex(space, {.kind = ForceField::Kind::Vortex, .position = {0.0F, 2000.0F}, .radius = 500.0F, .strength = 500.0F});
    Body leaf = ball(space, {200.0F, 2000.0F});
    simulate(space, 0.2F);
    EXPECT_GT(leaf.getVelocity().y, 50.0F);
}

TEST_F(ForceFieldTest, BuoyancyFloatsBodiesAtTheirDensity) {
    // In water of density 1, a box of density 0.5 floats with half its height under the surface, and a box of density 2 sinks.
    World world({.subSteps = 8});
    Body bottom = world.createBody({.type = Body::Type::Static, .position = {0.0F, 520.0F}});
    bottom.addBox({2000.0F, 40.0F});
    ForceField water(world, {.kind = ForceField::Kind::Buoyancy, .position = {0.0F, 250.0F}, .size = {2000.0F, 500.0F}, .density = 1.0F, .linearDrag = 2.0F, .angularDrag = 2.0F});
    Body cork = world.createBody({.position = {-200.0F, -100.0F}, .fixedRotation = true});
    cork.addBox({64.0F, 64.0F}, {.density = 0.5F});
    Body stone = world.createBody({.position = {200.0F, -100.0F}});
    stone.addBox({64.0F, 64.0F}, {.density = 2.0F});
    simulate(world, 8.0F);
    EXPECT_NEAR(cork.getPosition().y, 0.0F, 3.0F);
    EXPECT_NEAR(stone.getPosition().y, 468.0F, 3.0F);
    EXPECT_THROW(ForceField(world, {.kind = ForceField::Kind::Buoyancy, .radius = 100.0F}), std::invalid_argument);
}

} // namespace haylen::physics2d
