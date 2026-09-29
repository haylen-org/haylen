#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <vector>

#include "haylen/2d/procedural/Autotile.hpp"
#include "haylen/2d/procedural/Delaunay.hpp"
#include "haylen/2d/procedural/Voronoi.hpp"
#include "haylen/2d/procedural/WaveFunctionCollapse.hpp"
#include "haylen/2d/tiled/WangSet.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::procedural2d {

class TilingTest : public ::testing::Test {
  protected:
    static std::vector<math::Vec2> randomPoints(std::size_t count, std::uint64_t seed) {
        math::Random random(seed);
        std::vector<math::Vec2> points;
        for (std::size_t index = 0; index < count; ++index) {
            points.push_back({random.range(0.0F, 1000.0F), random.range(0.0F, 1000.0F)});
        }
        return points;
    }

    static float closestPair(const std::vector<math::Vec2>& sites) {
        float best = 1e9F;
        for (std::size_t first = 0; first < sites.size(); ++first) {
            for (std::size_t second = first + 1; second < sites.size(); ++second) {
                best = std::min(best, math::Vec2::distance(sites[first], sites[second]));
            }
        }
        return best;
    }
};

TEST_F(TilingTest, DelaunayTrianglesHaveEmptyCircumcircles) {
    std::vector<math::Vec2> points = randomPoints(300, 2);
    points.push_back(points[10]);
    const Delaunay delaunay(points);

    // A planar triangulation of n points with h on the hull has 2n - 2 - h triangles.
    EXPECT_EQ(delaunay.getTriangleCount(), 2 * 300 - 2 - delaunay.getHull().size());
    const std::vector<std::uint32_t>& triangles = delaunay.getTriangles();
    for (std::size_t triangle = 0; triangle < delaunay.getTriangleCount(); ++triangle) {
        const std::vector<math::Vec2> corners{points[triangles[triangle * 3]], points[triangles[triangle * 3 + 1]], points[triangles[triangle * 3 + 2]]};
        EXPECT_GT(math::Geometry::signedArea(corners), 0.0F);

        const math::Vec2 center = delaunay.getCircumcenter(triangle);
        const float radius = math::Vec2::distance(center, corners[0]);
        for (const math::Vec2 point : points) {
            EXPECT_GE(math::Vec2::distance(center, point), radius * (1.0F - 1e-4F));
        }
    }

    // Every inner edge pairs with the same edge running the other way in its neighbor.
    const std::vector<std::int32_t>& halfedges = delaunay.getHalfedges();
    for (std::size_t edge = 0; edge < halfedges.size(); ++edge) {
        if (halfedges[edge] < 0) {
            continue;
        }
        const auto twin = static_cast<std::size_t>(halfedges[edge]);
        EXPECT_EQ(static_cast<std::size_t>(halfedges[twin]), edge);
        EXPECT_EQ(triangles[edge], triangles[twin - twin % 3 + (twin + 1) % 3]);
    }

    // The duplicate point stays out, and walking the edges finds the nearest point.
    EXPECT_TRUE(delaunay.getNeighbors().back().empty());
    const math::Vec2 probe{500.0F, 500.0F};
    const auto nearest = std::min_element(points.begin(), points.end() - 1, [probe](math::Vec2 lhs, math::Vec2 rhs) { return math::Vec2::distanceSquared(lhs, probe) < math::Vec2::distanceSquared(rhs, probe); });
    EXPECT_EQ(delaunay.findNearest(probe), static_cast<std::uint32_t>(nearest - points.begin()));
}

TEST_F(TilingTest, DelaunayHandlesCollinearAndTinyInputs) {
    const Delaunay line({{0.0F, 0.0F}, {2.0F, 0.0F}, {1.0F, 0.0F}, {3.0F, 0.0F}});
    EXPECT_EQ(line.getTriangleCount(), 0U);
    EXPECT_EQ(line.getHull(), (std::vector<std::uint32_t>{0, 2, 1, 3}));
    EXPECT_EQ(line.getNeighbors()[2], (std::vector<std::uint32_t>{0, 1}));

    EXPECT_EQ(Delaunay({{1.0F, 1.0F}}).getHull().size(), 1U);
    EXPECT_EQ(Delaunay({}).getTriangleCount(), 0U);
    EXPECT_EQ(Delaunay({{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}}).getTriangleCount(), 1U);
}

TEST_F(TilingTest, VoronoiCellsTileTheBoundsAroundTheirSites) {
    const std::vector<math::Vec2> points = randomPoints(120, 5);
    const math::Rect bounds{0.0F, 0.0F, 1000.0F, 1000.0F};
    const Voronoi voronoi(points, bounds);

    float total = 0.0F;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const std::vector<math::Vec2>& cell = voronoi.getCells()[index];
        ASSERT_GE(cell.size(), 3U);
        EXPECT_TRUE(math::Geometry::isConvex(cell));
        EXPECT_GT(math::Geometry::signedArea(cell), 0.0F);
        EXPECT_TRUE(math::Geometry::contains(cell, points[index]));
        total += math::Geometry::signedArea(cell);
    }
    EXPECT_NEAR(total, bounds.getArea(), 10.0F);
    EXPECT_EQ(voronoi.findCell(points[7] + math::Vec2{0.01F, 0.0F}), 7U);

    // Lloyd relaxation spreads the points out, so the closest pair moves apart.
    EXPECT_GT(closestPair(Voronoi::relax(points, bounds, 5)), closestPair(points) * 2.0F);

    const Voronoi single({{10.0F, 10.0F}, {10.0F, 10.0F}}, bounds);
    EXPECT_NEAR(math::Geometry::signedArea(single.getCells()[0]), bounds.getArea(), 1e-3F);
    EXPECT_TRUE(single.getCells()[1].empty());
}

TEST_F(TilingTest, WaveFunctionCollapseFollowsTheRules) {
    // Land (0), coast (1) and sea (2): land never touches sea.
    WaveFunctionCollapse::Rules rules(3);
    for (const auto direction : {WaveFunctionCollapse::Direction::Right, WaveFunctionCollapse::Direction::Down}) {
        rules.allow(0, 0, direction);
        rules.allow(1, 1, direction);
        rules.allow(2, 2, direction);
        rules.allow(0, 1, direction);
        rules.allow(1, 0, direction);
        rules.allow(1, 2, direction);
        rules.allow(2, 1, direction);
    }
    EXPECT_TRUE(rules.isAllowed(1, 0, WaveFunctionCollapse::Direction::Left));
    EXPECT_FALSE(rules.isAllowed(0, 2, WaveFunctionCollapse::Direction::Up));

    math::Random random(14);
    const std::optional<spatial2d::CellGrid> map = WaveFunctionCollapse::generate(rules, {.width = 40, .height = 30}, random);
    ASSERT_TRUE(map.has_value());
    EXPECT_TRUE(WaveFunctionCollapse::isValid(*map, rules));
    const std::set<std::int32_t> used(map->getValues().begin(), map->getValues().end());
    EXPECT_EQ(used.size(), 3U);

    // Fixed cells keep their tile, and weights of zero keep a tile out.
    spatial2d::CellGrid fixed(10, 10, -1);
    fixed.set({0, 0}, 2);
    fixed.set({9, 9}, 0);
    rules.setWeight(1, 5.0F);
    const std::optional<spatial2d::CellGrid> constrained = WaveFunctionCollapse::generate(rules, {.width = 10, .height = 10, .periodic = true, .fixed = fixed}, random);
    ASSERT_TRUE(constrained.has_value());
    const spatial2d::CellGrid& island = *constrained;
    EXPECT_EQ((island[{0, 0}]), 2);
    EXPECT_EQ((island[{9, 9}]), 0);
    EXPECT_TRUE(WaveFunctionCollapse::isValid(*constrained, rules, true));

    // Land fixed right next to sea can never work.
    fixed.set({1, 0}, 0);
    EXPECT_FALSE(WaveFunctionCollapse::generate(rules, {.width = 10, .height = 10, .fixed = fixed}, random).has_value());
    EXPECT_THROW((void)WaveFunctionCollapse::generate(rules, {.width = 5, .height = 10, .fixed = fixed}, random), std::invalid_argument);
    EXPECT_THROW(rules.allow(0, 3, WaveFunctionCollapse::Direction::Right), std::out_of_range);
}

TEST_F(TilingTest, WaveFunctionCollapseLearnsFromASample) {
    // Stripes of 0 1 2 across, repeating down: 0 only sits left of 1, 1 left of 2, 2 left of 0 in a periodic sample.
    spatial2d::CellGrid sample(6, 4);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 6; ++x) {
            sample.set({x, y}, x % 3);
        }
    }
    const WaveFunctionCollapse::Rules rules = WaveFunctionCollapse::Rules::fromSample(sample, true);
    EXPECT_EQ(rules.getTileCount(), 3U);
    EXPECT_TRUE(rules.isAllowed(2, 0, WaveFunctionCollapse::Direction::Right));
    EXPECT_FALSE(rules.isAllowed(0, 2, WaveFunctionCollapse::Direction::Right));
    EXPECT_FLOAT_EQ(rules.getWeight(1), 8.0F);

    math::Random random(2);
    const std::optional<spatial2d::CellGrid> output = WaveFunctionCollapse::generate(rules, {.width = 12, .height = 5, .periodic = true}, random);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(WaveFunctionCollapse::isValid(*output, rules, true));
    const spatial2d::CellGrid& stripes = *output;
    for (int x = 1; x < 12; ++x) {
        EXPECT_EQ((stripes[{x, 0}]), (stripes[{x - 1, 0}] + 1) % 3);
    }
    EXPECT_FALSE(WaveFunctionCollapse::isValid(sample, WaveFunctionCollapse::Rules(2)));

    spatial2d::CellGrid negative(1, 1, -1);
    EXPECT_THROW((void)WaveFunctionCollapse::Rules::fromSample(negative), std::invalid_argument);
}

TEST_F(TilingTest, AutotileMasksFollowTheNeighbors) {
    // A plus sign of 1 in a field of 0.
    spatial2d::CellGrid terrain(3, 3, 0);
    for (const spatial2d::Cell cell : {spatial2d::Cell{1, 0}, spatial2d::Cell{0, 1}, spatial2d::Cell{1, 1}, spatial2d::Cell{2, 1}, spatial2d::Cell{1, 2}}) {
        terrain.set(cell, 1);
    }
    EXPECT_EQ(Autotile::getMask4(terrain, {1, 1}), 15);
    EXPECT_EQ(Autotile::getMask4(terrain, {1, 0}), 1 | 4);
    EXPECT_EQ(Autotile::getMask4(terrain, {1, 0}, false), 4);
    // Diagonal neighbors of the center are 0, so only the four sides count.
    EXPECT_EQ(Autotile::getMask8(terrain, {1, 1}), 1 | 4 | 16 | 64);
    EXPECT_EQ(Autotile::getMask8(spatial2d::CellGrid(3, 3, 1), {1, 1}), 255);
    EXPECT_EQ(Autotile::getMask8(terrain, {0, 0}, false), 0);

    const spatial2d::CellGrid masks = Autotile::apply4(terrain, 1, false);
    EXPECT_EQ((masks[{0, 0}]), -1);
    EXPECT_EQ((masks[{0, 1}]), 2);

    std::set<int> indices;
    for (unsigned mask = 0; mask < 256; ++mask) {
        const int index = Autotile::getBlobIndex(static_cast<std::uint8_t>(mask));
        if (index >= 0) {
            indices.insert(index);
        }
    }
    EXPECT_EQ(indices.size(), 47U);
    EXPECT_EQ(Autotile::getBlobIndex(0), 0);
    EXPECT_EQ(Autotile::getBlobIndex(255), 46);
    EXPECT_EQ(Autotile::getBlobIndex(2), -1);
    EXPECT_EQ((Autotile::apply8(terrain, 1)[{1, 1}]), Autotile::getBlobIndex(1 | 4 | 16 | 64));
}

TEST_F(TilingTest, WangSetsPickTilesByCornersAndEdges) {
    // A corner set with grass (1) and water (2): tile 0 is all grass, tile 1 all water and tile 2 has water in the bottom right corner.
    tiled::WangSet corners{.kind = "corner"};
    corners.tiles = {{0, {0, 1, 0, 1, 0, 1, 0, 1}}, {1, {0, 2, 0, 2, 0, 2, 0, 2}}, {2, {0, 1, 0, 2, 0, 1, 0, 1}}, {7, {0, 1, 0, 2, 0, 1, 0, 1}}};
    spatial2d::CellGrid colors(3, 3, 1);
    colors.set({2, 2}, 2);
    const spatial2d::CellGrid tiles = Autotile::applyWang(colors, corners, 4);
    EXPECT_EQ(tiles.getWidth(), 2);
    EXPECT_EQ((tiles[{0, 0}]), 0);
    EXPECT_TRUE((tiles[{1, 1}]) == 2 || (tiles[{1, 1}]) == 7);
    EXPECT_EQ((tiles[{1, 1}]), (Autotile::applyWang(colors, corners, 4)[{1, 1}]));

    colors.set({0, 0}, 2);
    EXPECT_EQ((Autotile::applyWang(colors, corners)[{0, 0}]), -1);

    // An edge set of roads (1): a straight road east to west and a dead end.
    tiled::WangSet roads{.kind = "edge"};
    roads.tiles = {{10, {0, 0, 1, 0, 0, 0, 1, 0}}, {11, {0, 0, 1, 0, 0, 0, 0, 0}}};
    spatial2d::CellGrid map(3, 1, 1);
    map.set({2, 0}, 0);
    const spatial2d::CellGrid roadTiles = Autotile::applyWang(map, roads);
    EXPECT_EQ((roadTiles[{0, 0}]), 11);
    EXPECT_EQ((roadTiles[{1, 0}]), -1);
    EXPECT_EQ((roadTiles[{2, 0}]), -1);

    const tiled::WangSet unknown{.kind = "diagonal"};
    EXPECT_THROW((void)Autotile::applyWang(map, unknown), std::invalid_argument);
}

} // namespace haylen::procedural2d
