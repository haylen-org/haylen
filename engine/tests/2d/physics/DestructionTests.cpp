#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "haylen/2d/physics/Explosion.hpp"
#include "haylen/2d/physics/Fracture.hpp"
#include "haylen/2d/physics/Terrain.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Polygon.hpp"

namespace haylen::physics2d {

class DestructionTest : public ::testing::Test {
  protected:
    void simulate(float seconds) {
        for (int step = 0; step < static_cast<int>(seconds * 60.0F); ++step) {
            world.step(1.0F / 60.0F);
        }
    }

    Body crate(math::Vec2 position, float size = 20.0F) {
        Body body = world.createBody({.position = position});
        body.addBox({size, size});
        return body;
    }

    World world;
};

TEST_F(DestructionTest, TerrainBuildsChainsFromFilledShapes) {
    // A 512 by 256 area of 4 unit cells in chunks of 32 cells, filled below y = 102, between two rows of samples.
    Terrain terrain(world, {.columns = 129, .rows = 65, .cellSize = 4.0F});
    EXPECT_EQ(terrain.getChunkCount(), 8U);
    const std::vector<math::Vec2> floor{{0.0F, 102.0F}, {512.0F, 102.0F}, {512.0F, 256.0F}, {0.0F, 256.0F}};
    terrain.fill(floor);
    EXPECT_EQ(terrain.getDirtyChunkCount(), 8U);
    EXPECT_EQ(terrain.update(), 8U);
    EXPECT_EQ(terrain.update(), 0U);
    EXPECT_EQ(terrain.getBodies().size(), 8U);

    EXPECT_TRUE(terrain.isSolid({100.0F, 200.0F}));
    EXPECT_FALSE(terrain.isSolid({100.0F, 90.0F}));
    EXPECT_FALSE(terrain.isSolid({-10.0F, 200.0F}));
    EXPECT_EQ(terrain.getSample(10, 60), 255);
    EXPECT_EQ(terrain.getSample(10, 10), 0);

    // Outlines are simplified to few points and sit on the filled edge.
    float area = 0.0F;
    for (const std::vector<math::Vec2>& outline : terrain.getOutlines()) {
        EXPECT_GE(outline.size(), 4U);
        EXPECT_GT(math::Geometry::signedArea(outline), 0.0F);
        area += math::Geometry::signedArea(outline);
        for (const math::Vec2 point : outline) {
            EXPECT_GE(point.y, 101.9F);
        }
    }
    // The two upper corners sit on the edge of the grid, where samples are half covered, and lose a small triangle each.
    EXPECT_NEAR(area, 512.0F * 154.0F, 16.0F);

    // A ball dropped onto the ground comes to rest on its surface.
    Body ball = world.createBody({.position = {100.0F, 50.0F}});
    ball.addCircle(8.0F);
    simulate(2.0F);
    EXPECT_NEAR(ball.getPosition().y, 94.0F, 1.5F);
}

TEST_F(DestructionTest, CarvingRebuildsOnlyTheChunksItTouches) {
    Terrain terrain(world, {.columns = 129, .rows = 65, .cellSize = 4.0F, .chunkSize = 32});
    std::vector<std::uint8_t> solid(129U * 65U, 0);
    std::fill(solid.begin() + 129 * 20, solid.end(), std::uint8_t{255});
    terrain.setSamples(solid);
    EXPECT_EQ(terrain.update(), 8U);
    const Body untouched = terrain.getBodies().back();

    // A crater inside the first chunk column.
    terrain.carve(math::Circle{{40.0F, 80.0F}, 20.0F});
    EXPECT_EQ(terrain.getDirtyChunkCount(), 1U);
    EXPECT_EQ(terrain.update(), 1U);
    EXPECT_FALSE(terrain.isSolid({40.0F, 90.0F}));
    EXPECT_TRUE(terrain.isSolid({40.0F, 110.0F}));
    EXPECT_TRUE(untouched.isValid());

    // A crater across a chunk edge changes both chunks, and their outlines still meet at the edge. The surface lies halfway between rows 19 and 20, at y = 78, so each crater also cuts a band 2 units deep above its center.
    terrain.carve(math::Circle{{128.0F, 80.0F}, 24.0F});
    EXPECT_EQ(terrain.update(), 2U);
    float area = 0.0F;
    for (const std::vector<math::Vec2>& outline : terrain.getOutlines()) {
        area += math::Geometry::signedArea(outline);
    }
    const float craters = 3.14159265F * (20.0F * 20.0F + 24.0F * 24.0F) * 0.5F + 4.0F * (20.0F + 24.0F);
    EXPECT_NEAR(area, 512.0F * (256.0F - 78.0F) - craters, craters * 0.1F);

    // A ball dropped into the crater falls below the untouched ground level.
    Body ball = world.createBody({.position = {128.0F, 20.0F}});
    ball.addCircle(6.0F);
    simulate(2.0F);
    EXPECT_GT(ball.getPosition().y, 85.0F);

    // Filling the crater back closes it.
    terrain.fill(math::Circle{{128.0F, 80.0F}, 30.0F});
    EXPECT_EQ(terrain.update(), 2U);
    EXPECT_TRUE(terrain.isSolid({128.0F, 90.0F}));
    EXPECT_THROW(terrain.setSamples(std::vector<std::uint8_t>(3)), std::invalid_argument);
    EXPECT_THROW((void)terrain.getSample(200, 0), std::out_of_range);
    EXPECT_THROW(Terrain(world, {.columns = 1}), std::invalid_argument);
    EXPECT_THROW(Terrain(world, {.shape = {.friction = -1.0F}}), std::invalid_argument);
}

TEST_F(DestructionTest, TerrainExplosionsCarveAndPush) {
    Terrain terrain(world, {.columns = 65, .rows = 33, .cellSize = 8.0F});
    terrain.fill(std::vector<math::Vec2>{{0.0F, 128.0F}, {512.0F, 128.0F}, {512.0F, 256.0F}, {0.0F, 256.0F}});
    terrain.update();
    Body near = crate({200.0F, 110.0F});
    simulate(0.5F);

    const std::vector<Explosion::Hit> hits = terrain.explode({{180.0F, 140.0F}, 30.0F}, {.center = {180.0F, 140.0F}, .radius = 80.0F, .impulse = 800.0F});
    ASSERT_EQ(hits.size(), 1U);
    EXPECT_EQ(hits.front().body, near);
    EXPECT_FALSE(terrain.isSolid({180.0F, 150.0F}));
    EXPECT_GT(near.getVelocity().x, 0.0F);
    EXPECT_LT(near.getVelocity().y, 0.0F);
}

TEST_F(DestructionTest, ExplosionsFadeWithDistanceAndHideBehindWalls) {
    world.setGravity({});
    Body close = crate({40.0F, 0.0F});
    Body far = crate({100.0F, -60.0F});
    Body behind = crate({0.0F, 80.0F});
    Body wall = world.createBody({.type = Body::Type::Static, .position = {0.0F, 40.0F}});
    wall.addBox({100.0F, 10.0F});

    const std::vector<Explosion::Hit> hits = Explosion::apply(world, {.center = {0.0F, 0.0F}, .radius = 200.0F, .impulse = 100.0F, .occlusion = true});
    ASSERT_EQ(hits.size(), 2U);
    EXPECT_GT(close.getVelocity().x, far.getVelocity().x);
    EXPECT_GT(far.getVelocity().x, 0.0F);
    EXPECT_TRUE(behind.getVelocity().isZero());
    for (const Explosion::Hit& hit : hits) {
        EXPECT_LE(hit.impulse.getLength(), 100.0F);
    }

    // Without occlusion the crate behind the wall moves too, and without falloff every crate takes the full impulse.
    const std::vector<Explosion::Hit> open = Explosion::apply(world, {.center = {0.0F, 0.0F}, .radius = 200.0F, .impulse = 100.0F, .falloff = Explosion::Falloff::None});
    EXPECT_EQ(open.size(), 3U);
    for (const Explosion::Hit& hit : open) {
        EXPECT_NEAR(hit.impulse.getLength(), 100.0F, 1e-3F);
    }
    EXPECT_GT(behind.getVelocity().y, 0.0F);

    EXPECT_EQ(Explosion::falloffFromName("quadratic"), Explosion::Falloff::Quadratic);
    EXPECT_EQ(Explosion::falloffName(Explosion::Falloff::None), "none");
    EXPECT_THROW((void)Explosion::apply(world, {.radius = 0.0F}), std::invalid_argument);
}

TEST_F(DestructionTest, ExplosionsSeeThroughSensorsAndShapesTheirFilterSkips) {
    world.setGravity({});
    Body belowSensor = crate({0.0F, 80.0F});
    Body aboveGlass = crate({0.0F, -80.0F});
    Body zone = world.createBody({.type = Body::Type::Static, .position = {0.0F, 40.0F}});
    zone.addBox({100.0F, 10.0F}, {.sensor = true});
    Body glass = world.createBody({.type = Body::Type::Static, .position = {0.0F, -40.0F}});
    glass.addBox({100.0F, 10.0F}, {.filter = {.category = 2}});

    const std::vector<Explosion::Hit> hits = Explosion::apply(world, {.center = {0.0F, 0.0F}, .radius = 200.0F, .impulse = 100.0F, .occlusion = true, .filter = {.mask = 1}});
    EXPECT_EQ(hits.size(), 2U);
    EXPECT_GT(belowSensor.getVelocity().y, 0.0F);
    EXPECT_LT(aboveGlass.getVelocity().y, 0.0F);
}

TEST_F(DestructionTest, FracturesSplitShapesIntoCoveringPieces) {
    const std::vector<std::vector<math::Vec2>> square{{{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}}};
    const auto pieces = Fracture::split(square, {.pieces = 12, .impact = math::Vec2{20.0F, 30.0F}, .seed = 3, .minimumArea = 0.0F});
    ASSERT_GE(pieces.size(), 10U);
    float area = 0.0F;
    for (const auto& piece : pieces) {
        area += math::Polygon::getArea(piece);
    }
    EXPECT_NEAR(area, 10000.0F, 1.0F);
    EXPECT_THROW((void)Fracture::split(square, {.pieces = 0}), std::invalid_argument);
}

TEST_F(DestructionTest, ShatteringReplacesABodyWithMovingPieces) {
    world.setGravity({});
    Body box = world.createBody({.position = {50.0F, 50.0F}, .rotation = 0.3F, .velocity = {30.0F, 0.0F}});
    box.addBox({60.0F, 40.0F}, {.density = 2.0F, .friction = 0.2F});
    const float mass = box.getMass();
    const float rotation = box.getRotation();

    const std::vector<Body> fragments = Fracture::shatter(box, {.pieces = 6, .impact = math::Vec2{60.0F, 50.0F}, .seed = 9});
    EXPECT_FALSE(box.isValid());
    ASSERT_GE(fragments.size(), 4U);
    float total = 0.0F;
    for (const Body& fragment : fragments) {
        total += fragment.getMass();
        EXPECT_NEAR(fragment.getVelocity().x, 30.0F, 1e-3F);
        EXPECT_NEAR(fragment.getRotation(), rotation, 5e-3F);
    }
    EXPECT_NEAR(total, mass, mass * 0.01F);

    Body round = world.createBody({});
    round.addCircle(10.0F);
    EXPECT_TRUE(Fracture::shatter(round, {}).empty());
    EXPECT_TRUE(round.isValid());
    EXPECT_THROW((void)Fracture::shatter(box, {}), std::logic_error);

    // Pieces of a sliver are thinner than the tolerance of Box2D, so the fracture fails and leaves the sliver whole.
    Body sliver = world.createBody({.position = {0.0F, 300.0F}});
    sliver.addBox({100.0F, 0.5F});
    const std::size_t bodies = world.getBodyCount();
    EXPECT_THROW((void)Fracture::shatter(sliver, {.pieces = 4}), std::invalid_argument);
    EXPECT_TRUE(sliver.isValid());
    EXPECT_EQ(world.getBodyCount(), bodies);
}

} // namespace haylen::physics2d
