#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "haylen/2d/physics/Fluid.hpp"
#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

class FluidTest : public ::testing::Test {
  protected:
    void simulate(float seconds) {
        for (int step = 0; step < static_cast<int>(seconds * 60.0F); ++step) {
            world.step(1.0F / 60.0F);
        }
    }

    void tank(float width, float height, float thickness = 20.0F) {
        Body walls = world.createBody({.type = Body::Type::Static});
        walls.addBox({width + thickness * 2.0F, thickness}, {.offset = {0.0F, thickness * 0.5F}});
        walls.addBox({thickness, height}, {.offset = {-(width + thickness) * 0.5F, -height * 0.5F}});
        walls.addBox({thickness, height}, {.offset = {(width + thickness) * 0.5F, -height * 0.5F}});
    }

    // Returns the number of particles outside a tank of this width whose floor is at zero.
    static std::size_t countOutside(const Fluid& fluid, float width) {
        return static_cast<std::size_t>(std::count_if(fluid.getPositions().begin(), fluid.getPositions().end(), [width](math::Vec2 position) { return position.y > 0.0F || std::abs(position.x) > width * 0.5F; }));
    }

    static float getFastest(const Fluid& fluid) {
        float fastest = 0.0F;
        for (const math::Vec2 velocity : fluid.getVelocities()) {
            fastest = std::max(fastest, velocity.getLength());
        }
        return fastest;
    }

    static float getTop(const Fluid& fluid) {
        float top = 0.0F;
        for (const math::Vec2 position : fluid.getPositions()) {
            top = std::min(top, position.y);
        }
        return top;
    }

    World world;
};

TEST_F(FluidTest, SpreadsAndSettlesInsideItsContainer) {
    tank(400.0F, 300.0F);
    Fluid fluid(world, {.radius = 4.0F, .smoothingRadius = 16.0F, .maxParticles = 300});
    EXPECT_EQ(fluid.fill({-190.0F, -200.0F, 160.0F, 200.0F}), 300U);
    EXPECT_FALSE(fluid.spawn({0.0F, 0.0F}));
    EXPECT_EQ(fluid.size(), 300U);
    simulate(6.0F);

    // The block collapses over the floor between the walls and comes to rest, without passing the floor.
    float left = 1e9F;
    float right = -1e9F;
    for (std::size_t index = 0; index < fluid.size(); ++index) {
        const math::Vec2 position = fluid.getPositions()[index];
        EXPECT_GT(position.x, -200.0F);
        EXPECT_LT(position.x, 200.0F);
        EXPECT_LT(position.y, 0.5F);
        EXPECT_GT(position.y, -150.0F);
        EXPECT_LT(fluid.getVelocities()[index].getLength(), 120.0F);
        left = std::min(left, position.x);
        right = std::max(right, position.x);
    }
    EXPECT_GT(right - left, 330.0F);
    EXPECT_GE(fluid.getStepMilliseconds(), 0.0F);

    fluid.remove(0);
    EXPECT_EQ(fluid.size(), 299U);
    EXPECT_THROW(fluid.remove(500), std::out_of_range);
    fluid.clear();
    EXPECT_EQ(fluid.size(), 0U);
    simulate(0.1F);
    EXPECT_THROW(Fluid(world, {.radius = 4.0F, .smoothingRadius = 2.0F}), std::invalid_argument);
    EXPECT_THROW(Fluid(world, {.friction = -1.0F}), std::invalid_argument);
}

TEST_F(FluidTest, FloatsLightBodiesAndSinksHeavyOnes) {
    // Particles push the bodies they hit, so a crate lighter than the liquid floats on it and a denser one goes to the bottom.
    tank(600.0F, 500.0F);
    Fluid fluid(world, {.radius = 4.0F, .smoothingRadius = 16.0F, .density = 1.0F, .maxParticles = 6000});
    fluid.fill({-300.0F, -300.0F, 600.0F, 300.0F});
    simulate(3.0F);
    float surface = 0.0F;
    for (const math::Vec2 position : fluid.getPositions()) {
        surface = std::min(surface, position.y);
    }

    Body cork = world.createBody({.position = {-150.0F, surface - 80.0F}});
    cork.addBox({60.0F, 40.0F}, {.density = 0.3F});
    Body stone = world.createBody({.position = {150.0F, surface - 80.0F}});
    stone.addBox({40.0F, 40.0F}, {.density = 4.0F});
    simulate(5.0F);

    EXPECT_LT(cork.getPosition().y, surface + 25.0F);
    EXPECT_GT(stone.getPosition().y, -60.0F);
}

TEST_F(FluidTest, RestsWithoutGainingEnergyOrVolume) {
    // A deep tank settles and stays still: the bottom holds the weight of the column without squeezing the liquid to half its depth, and no particle is thrown out through the floor.
    tank(600.0F, 400.0F);
    Fluid fluid(world, {.radius = 4.0F, .smoothingRadius = 16.0F, .viscosity = 0.25F, .maxParticles = 4000});
    const std::size_t added = fluid.fill({-300.0F, -200.0F, 600.0F, 200.0F});
    const float depth = static_cast<float>(added) * 64.0F / 600.0F;
    for (int second = 0; second < 6; ++second) {
        simulate(1.0F);
        ASSERT_EQ(countOutside(fluid, 600.0F), 0U) << "Second " << second;
    }
    EXPECT_LT(getFastest(fluid), 15.0F);
    EXPECT_LT(getTop(fluid), -depth * 0.85F);
}

TEST_F(FluidTest, SlowTouchesNeverSplash) {
    // A paddle that sinks slowly into still liquid and a floating crate dragged slowly through it only raise the surface by what they displace and the small waves they make, and the liquid calms down once they stop.
    tank(600.0F, 400.0F);
    Fluid fluid(world, {.radius = 4.0F, .smoothingRadius = 16.0F, .viscosity = 0.25F, .maxParticles = 4000});
    fluid.fill({-300.0F, -160.0F, 600.0F, 160.0F});
    simulate(4.0F);
    const float surface = getTop(fluid);
    Body crate = world.createBody({.position = {-20.0F, surface - 30.0F}});
    crate.addBox({70.0F, 50.0F}, {.density = 0.4F});
    simulate(2.0F);

    Body paddle = world.createBody({.type = Body::Type::Kinematic, .position = {-200.0F, surface - 30.0F}, .velocity = {0.0F, 40.0F}});
    paddle.addBox({70.0F, 50.0F});
    float top = 0.0F;
    for (int step = 0; step < 120; ++step) {
        crate.setVelocity({40.0F, crate.getVelocity().y});
        world.step(1.0F / 60.0F);
        top = std::min(top, getTop(fluid));
    }
    EXPECT_GT(top, surface - 20.0F);
    EXPECT_LT(std::abs(crate.getAngularVelocity()), 1.0F);

    paddle.setVelocity({});
    simulate(3.0F);
    EXPECT_LT(getFastest(fluid), 40.0F);
    EXPECT_EQ(countOutside(fluid, 600.0F), 0U);
}

TEST_F(FluidTest, FloatsVeryLightBodiesCalmly) {
    // Many particles push a light body at once, and their pushes together never throw it back faster than they hit it, so crates far lighter than the liquid float still, lighter ones higher.
    tank(600.0F, 400.0F);
    Fluid fluid(world, {.radius = 4.0F, .smoothingRadius = 16.0F, .viscosity = 0.25F, .maxParticles = 4000});
    fluid.fill({-300.0F, -160.0F, 600.0F, 160.0F});
    simulate(3.0F);
    const float surface = getTop(fluid);
    std::vector<Body> crates;
    for (const float density : {0.05F, 0.2F, 0.5F}) {
        Body crate = world.createBody({.position = {-200.0F + static_cast<float>(crates.size()) * 200.0F, surface - 30.0F}});
        crate.addBox({70.0F, 50.0F}, {.density = density});
        crates.push_back(crate);
    }
    simulate(5.0F);
    for (const Body& crate : crates) {
        EXPECT_LT(crate.getVelocity().getLength(), 40.0F);
        EXPECT_LT(std::abs(crate.getAngularVelocity()), 1.0F);
        EXPECT_NEAR(crate.getPosition().y, surface, 40.0F);
    }
    EXPECT_LT(crates[0].getPosition().y, crates[2].getPosition().y);
    EXPECT_EQ(countOutside(fluid, 600.0F), 0U);
}

TEST_F(FluidTest, ThinWallsHoldFastParticlesAtEveryStep) {
    // Particles at the top speed cross a wall thinner than themselves within one step, and the collision along their path stops them at every step length.
    for (const float stepSeconds : {1.0F / 30.0F, 1.0F / 60.0F, 1.0F / 120.0F}) {
        World walled;
        Body walls = walled.createBody({.type = Body::Type::Static});
        walls.addBox({2.0F, 400.0F}, {.offset = {-200.0F, -200.0F}});
        walls.addSegment({200.0F, -400.0F}, {200.0F, 0.0F});
        walls.addBox({402.0F, 2.0F}, {.offset = {0.0F, 1.0F}});
        Fluid fluid(walled, {.radius = 4.0F, .smoothingRadius = 16.0F, .maxParticles = 200});
        for (int index = 0; index < 40; ++index) {
            const float y = -20.0F - static_cast<float>(index) * 9.0F;
            fluid.spawn({0.0F, y}, {index % 2 == 0 ? -3000.0F : 3000.0F, 0.0F});
            fluid.spawn({static_cast<float>(index) * 9.0F - 180.0F, -360.0F}, {0.0F, 3000.0F});
        }
        for (float time = 0.0F; time < 1.0F; time += stepSeconds) {
            walled.step(stepSeconds);
        }
        EXPECT_EQ(countOutside(fluid, 400.0F), 0U) << "Step " << stepSeconds;
    }
}

TEST_F(FluidTest, StaysOutOfOneSidedWallsAndSlopes) {
    // Chains hold the liquid on their front side, and particles slide down a slope into the basin.
    Body basin = world.createBody({.type = Body::Type::Static});
    basin.addChain(std::vector<math::Vec2>{{-300.0F, -400.0F}, {-300.0F, 0.0F}, {300.0F, 0.0F}, {300.0F, -400.0F}}, false);
    Body slope = world.createBody({.type = Body::Type::Static, .position = {-100.0F, -300.0F}, .rotation = 0.4F});
    slope.addBox({300.0F, 10.0F});
    Fluid fluid(world, {.maxParticles = 800});
    fluid.fill({-200.0F, -460.0F, 160.0F, 120.0F});
    simulate(4.0F);
    for (const math::Vec2 position : fluid.getPositions()) {
        EXPECT_GT(position.x, -300.0F);
        EXPECT_LT(position.x, 300.0F);
        EXPECT_LT(position.y, 0.5F);
    }
}

} // namespace haylen::physics2d
