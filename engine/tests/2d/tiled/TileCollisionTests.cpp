#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "haylen/2d/physics/World.hpp"
#include "haylen/2d/tiled/TileCollision.hpp"
#include "haylen/math/Geometry.hpp"

namespace haylen::tiled {

class TileCollisionTest : public ::testing::Test {
  protected:
    static constexpr float kCell = 32.0F;

    // Returns the outlines of the body sorted by their signed area, holes first.
    [[nodiscard]] std::vector<physics2d::Shape::Outline> outlines() const {
        std::vector<physics2d::Shape::Outline> result = floor.getOutlines();
        std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) { return math::Geometry::signedArea(lhs.points) < math::Geometry::signedArea(rhs.points); });
        return result;
    }

    void fillRow(int row, int first, int last) {
        for (int column = first; column <= last; ++column) {
            collision.setCell(column, row, {.full = true});
        }
    }

    physics2d::World world;
    physics2d::Body floor = world.createBody({.type = physics2d::Body::Type::Static});
    TileCollision collision{floor, 1, {}, {kCell, kCell}, {}};
};

TEST_F(TileCollisionTest, BodiesSlideAcrossTheJointsOfMergedTiles) {
    // A box sliding over a floor of one box per tile catches on the first joint, and over the merged floor it keeps its speed.
    physics2d::World separate;
    physics2d::Body tiles = separate.createBody({.type = physics2d::Body::Type::Static});
    for (int column = -5; column < 100; ++column) {
        (void)tiles.addBox({kCell, kCell}, {.offset = {static_cast<float>(column) * kCell + kCell * 0.5F, 10.0F * kCell + kCell * 0.5F}});
    }
    fillRow(10, -5, 99);
    collision.update();

    for (physics2d::World* space : {&separate, &world}) {
        physics2d::Body box = space->createBody({.position = {0.0F, 10.0F * kCell - 15.0F}, .velocity = {600.0F, 0.0F}, .fixedRotation = true});
        (void)box.addBox({30.0F, 30.0F}, {.friction = 0.0F});
        for (int step = 0; step < 120; ++step) {
            space->step(1.0F / 60.0F);
        }
        EXPECT_EQ(box.getVelocity().x > 599.0F, space == &world);
    }
}

TEST_F(TileCollisionTest, MergesTouchingCellsAndTracesHoles) {
    // A ring of eight cells around an empty one gives a loop around the ring and a loop around the hole, and a lone cell gives its own loop.
    fillRow(0, 0, 2);
    collision.setCell(0, 1, {.full = true});
    collision.setCell(2, 1, {.full = true});
    fillRow(2, 0, 2);
    collision.setCell(6, 0, {.full = true});
    collision.update();
    EXPECT_EQ(collision.getRegionCount(), 2U);

    const std::vector<physics2d::Shape::Outline> loops = outlines();
    ASSERT_EQ(loops.size(), 3U);
    EXPECT_NEAR(math::Geometry::signedArea(loops[0].points), -kCell * kCell, 0.1F);
    EXPECT_NEAR(math::Geometry::signedArea(loops[1].points), kCell * kCell, 0.1F);
    EXPECT_NEAR(math::Geometry::signedArea(loops[2].points), 9.0F * kCell * kCell, 0.1F);
    for (const physics2d::Shape::Outline& loop : loops) {
        EXPECT_TRUE(loop.closed);
        EXPECT_EQ(loop.points.size(), 4U);
    }

    // The loop around the hole holds a ball thrown inside it.
    physics2d::World space({.gravity = {}});
    physics2d::Body ground = space.createBody({.type = physics2d::Body::Type::Static});
    TileCollision ring(ground, 1, {}, {kCell, kCell}, {});
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            ring.setCell(column, row, {.full = row != 1 || column != 1});
        }
    }
    ring.update();
    physics2d::Body ball = space.createBody({.position = {kCell * 1.5F, kCell * 1.5F}, .velocity = {900.0F, 400.0F}});
    (void)ball.addCircle(4.0F, {.restitution = 1.0F});
    for (int step = 0; step < 120; ++step) {
        space.step(1.0F / 60.0F);
        const math::Rect hole{kCell, kCell, kCell, kCell};
        EXPECT_TRUE(hole.contains(ball.getPosition()));
    }
}

TEST_F(TileCollisionTest, CellsTouchingAtACornerKeepSeparateLoops) {
    collision.setCell(0, 0, {.full = true});
    collision.setCell(1, 1, {.full = true});
    collision.update();
    EXPECT_EQ(collision.getRegionCount(), 1U);
    const std::vector<physics2d::Shape::Outline> loops = outlines();
    ASSERT_EQ(loops.size(), 2U);
    EXPECT_EQ(loops[0].points.size(), 4U);
    EXPECT_EQ(loops[1].points.size(), 4U);
}

TEST_F(TileCollisionTest, SplitsAndJoinsRegionsWhenCellsChange) {
    fillRow(4, 0, 9);
    collision.update();
    EXPECT_EQ(collision.getRegionCount(), 1U);
    EXPECT_TRUE(world.raycast({5.5F * kCell, 3.0F * kCell}, {5.5F * kCell, 6.0F * kCell}).has_value());

    // Digging a cell splits the floor into two loops and opens a gap that rays pass.
    collision.setCell(5, 4, {});
    collision.update();
    EXPECT_EQ(collision.getRegionCount(), 2U);
    EXPECT_EQ(floor.getOutlines().size(), 2U);
    EXPECT_FALSE(world.raycast({5.5F * kCell, 3.0F * kCell}, {5.5F * kCell, 6.0F * kCell}).has_value());

    // Filling it again joins them, and a cell on top grows the same loop.
    collision.setCell(5, 4, {.full = true});
    collision.setCell(2, 3, {.full = true});
    collision.update();
    EXPECT_EQ(collision.getRegionCount(), 1U);
    const std::vector<physics2d::Shape::Outline> loops = outlines();
    ASSERT_EQ(loops.size(), 1U);
    EXPECT_EQ(loops[0].points.size(), 8U);
}

TEST_F(TileCollisionTest, MergesSlopesWithTheGround) {
    // A slope rising to the right on top of a floor of three cells becomes one outline, so a body walks from the floor up the slope without meeting a joint.
    fillRow(1, 0, 2);
    collision.setCell(1, 0, {.outlines = {{{kCell, kCell}, {kCell * 2.0F, 0.0F}, {kCell * 2.0F, kCell}}}});
    collision.update();
    EXPECT_EQ(collision.getRegionCount(), 1U);
    const std::vector<physics2d::Shape::Outline> loops = outlines();
    ASSERT_EQ(loops.size(), 1U);
    EXPECT_EQ(loops[0].points.size(), 7U);
    EXPECT_NEAR(math::Geometry::signedArea(loops[0].points), 3.5F * kCell * kCell, 0.1F);

    // A lone triangle has too few corners for a loop and stays a polygon.
    collision.setCell(8, 0, {.outlines = {{{8.0F * kCell, kCell}, {9.0F * kCell, 0.0F}, {9.0F * kCell, kCell}}}});
    collision.update();
    EXPECT_EQ(collision.getRegionCount(), 2U);
    ASSERT_EQ(world.queryPoint({8.8F * kCell, 0.8F * kCell}).size(), 1U);
    EXPECT_EQ(world.queryPoint({8.8F * kCell, 0.8F * kCell}).front().getKind(), physics2d::Shape::Kind::Polygon);
}

TEST_F(TileCollisionTest, DestroysTheSeparateShapesOfACellThatChanges) {
    physics2d::Shape sensor = floor.addBox({kCell, kCell}, {.sensor = true, .offset = {kCell * 0.5F, kCell * 0.5F}});
    collision.setCell(0, 0, {.shapes = {sensor}});
    collision.update();
    EXPECT_EQ(collision.getRegionCount(), 0U);
    EXPECT_TRUE(sensor.isValid());
    collision.setCell(0, 0, {.full = true});
    collision.update();
    EXPECT_FALSE(sensor.isValid());
    EXPECT_EQ(collision.getRegionCount(), 1U);
}

} // namespace haylen::tiled
