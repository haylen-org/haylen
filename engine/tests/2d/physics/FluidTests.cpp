#include <gtest/gtest.h>

#include <algorithm>
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

    void tank(float width, float height) {
        Body walls = world.createBody({.type = Body::Type::Static});
        walls.addBox({width + 40.0F, 20.0F}, {.offset = {0.0F, 10.0F}});
        walls.addBox({20.0F, height}, {.offset = {-width * 0.5F - 10.0F, -height * 0.5F}});
        walls.addBox({20.0F, height}, {.offset = {width * 0.5F + 10.0F, -height * 0.5F}});
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
