#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "haylen/2d/procedural/CellularAutomaton.hpp"
#include "haylen/2d/procedural/DrunkardWalk.hpp"
#include "haylen/2d/procedural/Dungeon.hpp"
#include "haylen/2d/procedural/Maze.hpp"
#include "haylen/2d/procedural/Region.hpp"
#include "haylen/2d/procedural/Scatter.hpp"
#include "haylen/2d/spatial/ConnectedComponents.hpp"
#include "haylen/2d/tiled/Object.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Noise2D.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::procedural2d {

class Procedural2DTest : public ::testing::Test {
  protected:
    // Counts the regions of cells holding zero, joined through their sides.
    static std::size_t countFloorRegions(const spatial2d::CellGrid& grid) {
        spatial2d::CellGrid floors(grid.getWidth(), grid.getHeight());
        for (int y = 0; y < grid.getHeight(); ++y) {
            for (int x = 0; x < grid.getWidth(); ++x) {
                floors.set({x, y}, grid[{x, y}] == 0 ? 1 : 0);
            }
        }
        spatial2d::CellGrid labels(grid.getWidth(), grid.getHeight());
        return spatial2d::ConnectedComponents::label(floors, {.background = 0}, labels);
    }

    static std::size_t countValue(const spatial2d::CellGrid& grid, std::int32_t value) {
        return static_cast<std::size_t>(std::count(grid.getValues().begin(), grid.getValues().end(), value));
    }
};

TEST_F(Procedural2DTest, ScatterCountsMatchTheAreaTimesTheDensity) {
    const std::vector<Region> regions{
        Region::rect({0.0F, 0.0F, 1000.0F, 500.0F}),
        Region::circle({0.0F, 0.0F}, 300.0F),
        Region::ring({0.0F, 0.0F}, 200.0F, 400.0F),
        Region::polygon(std::vector<std::vector<math::Vec2>>{{{0.0F, 0.0F}, {800.0F, 0.0F}, {400.0F, 600.0F}}}),
    };
    for (const Region& region : regions) {
        math::Random random(4);
        const std::vector<Scatter::Point> points = Scatter::generate(region, {.density = 0.002F}, random);
        const float expected = region.getArea() * 0.002F;
        EXPECT_NEAR(static_cast<float>(points.size()), expected, 1.0F);
        for (const Scatter::Point& point : points) {
            EXPECT_TRUE(region.contains(point.position));
        }
    }
    EXPECT_NEAR(Region::ring({0.0F, 0.0F}, 200.0F, 400.0F).getArea(), math::Math::kPi * (160000.0F - 40000.0F), 1.0F);
    EXPECT_NEAR(regions[3].getArea(), 240000.0F, 1.0F);
}

TEST_F(Procedural2DTest, RandomPolygonPointsAreUniform) {
    // A square with a square hole in its right half: points must avoid the hole and fill both halves evenly.
    const std::vector<std::vector<math::Vec2>> shape{
        {{0.0F, 0.0F}, {200.0F, 0.0F}, {200.0F, 100.0F}, {0.0F, 100.0F}},
        {{120.0F, 20.0F}, {120.0F, 80.0F}, {180.0F, 80.0F}, {180.0F, 20.0F}},
    };
    const Region region = Region::polygon(shape);
    EXPECT_NEAR(region.getArea(), 20000.0F - 3600.0F, 1.0F);

    math::Random random(8);
    int left = 0;
    for (int sample = 0; sample < 20000; ++sample) {
        const math::Vec2 point = region.getRandomPoint(random);
        ASSERT_TRUE(region.contains(point));
        left += point.x < 100.0F ? 1 : 0;
    }
    EXPECT_NEAR(static_cast<float>(left) / 20000.0F, 10000.0F / 16400.0F, 0.02F);
    EXPECT_THROW((void)Region::polygon(std::vector<std::vector<math::Vec2>>{{{0.0F, 0.0F}, {1.0F, 1.0F}, {2.0F, 2.0F}}}), std::invalid_argument);
}

TEST_F(Procedural2DTest, ScatterMethodsExclusionsAndWeights) {
    const Region area = Region::rect({0.0F, 0.0F, 400.0F, 400.0F});
    math::Random random(12);

    const std::vector<Scatter::Point> grid = Scatter::generate(area, {.method = Scatter::Method::Grid, .spacing = 40.0F, .jitter = 0.0F}, random);
    EXPECT_EQ(grid.size(), 100U);
    EXPECT_EQ(grid.front().position, math::Vec2(20.0F, 20.0F));

    const std::vector<Scatter::Point> poisson = Scatter::generate(area, {.method = Scatter::Method::Poisson, .spacing = 30.0F, .exclusions = {Region::circle({200.0F, 200.0F}, 80.0F)}}, random);
    EXPECT_GT(poisson.size(), 50U);
    for (std::size_t first = 0; first < poisson.size(); ++first) {
        EXPECT_GE(math::Vec2::distance(poisson[first].position, {200.0F, 200.0F}), 80.0F);
        for (std::size_t second = first + 1; second < poisson.size(); ++second) {
            EXPECT_GE(math::Vec2::distance(poisson[first].position, poisson[second].position), 30.0F - 1e-3F);
        }
    }

    const std::vector<Scatter::Point> typed = Scatter::generate(area, {.density = 0.05F, .weights = {3.0F, 1.0F}}, random);
    const auto rocks = std::count_if(typed.begin(), typed.end(), [](const Scatter::Point& point) { return point.type == 1; });
    EXPECT_NEAR(static_cast<float>(rocks) / static_cast<float>(typed.size()), 0.25F, 0.03F);

    EXPECT_THROW((void)Scatter::generate(area, {.spacing = 0.0F}, random), std::invalid_argument);
    EXPECT_THROW((void)Scatter::generate(area, {.layers = {{}}}, random), std::invalid_argument);
}

TEST_F(Procedural2DTest, BiomeLayersAndDensityMapsShapeTheScatter) {
    const Region area = Region::rect({0.0F, 0.0F, 400.0F, 400.0F});
    const math::Noise2D noise(3);
    math::Random random(5);

    Scatter::Options options{.density = 0.01F};
    options.biome = [&noise](math::Vec2 point) { return noise.fractal(point.x * 0.01F, point.y * 0.01F); };
    options.layers = {{.minimum = -1.0F, .maximum = 0.0F, .weights = {1.0F, 0.0F}}, {.minimum = 0.2F, .maximum = 1.0F, .weights = {0.0F, 1.0F}}};
    for (const Scatter::Point& point : Scatter::generate(area, options, random)) {
        const float value = options.biome(point.position);
        EXPECT_TRUE(value <= 0.0F || value >= 0.2F);
        EXPECT_EQ(point.type, value <= 0.0F ? 0U : 1U);
    }

    // The density map keeps points only on the left half, and Poisson spacing grows where the map drops.
    Scatter::Options dense{.density = 0.01F};
    dense.densityMap = [](math::Vec2 point) { return point.x < 200.0F ? 1.0F : 0.0F; };
    for (const Scatter::Point& point : Scatter::generate(area, dense, random)) {
        EXPECT_LT(point.position.x, 200.0F);
    }
    dense.method = Scatter::Method::Poisson;
    dense.spacing = 10.0F;
    dense.maximumSpacing = 40.0F;
    const std::vector<Scatter::Point> spaced = Scatter::generate(area, dense, random);
    const auto crowded = std::count_if(spaced.begin(), spaced.end(), [](const Scatter::Point& point) { return point.position.x < 200.0F; });
    EXPECT_GT(crowded, static_cast<std::ptrdiff_t>(spaced.size()) - crowded);
}

TEST_F(Procedural2DTest, RegionsComeFromTiledObjects) {
    tiled::Object box{.position = {10.0F, 20.0F}, .size = {30.0F, 40.0F}};
    EXPECT_EQ(Region::fromObject(box).getKind(), Region::Kind::Rect);
    EXPECT_FLOAT_EQ(Region::fromObject(box).getArea(), 1200.0F);

    box.rotation = math::Math::kHalfPi;
    const Region turned = Region::fromObject(box);
    EXPECT_EQ(turned.getKind(), Region::Kind::Polygon);
    EXPECT_NEAR(turned.getArea(), 1200.0F, 0.1F);
    EXPECT_TRUE(turned.contains({0.0F, 30.0F}));
    EXPECT_FALSE(turned.contains({20.0F, 30.0F}));

    const tiled::Object round{.position = {0.0F, 0.0F}, .size = {20.0F, 20.0F}, .shape = tiled::Object::Shape::Ellipse};
    EXPECT_EQ(Region::fromObject(round).getKind(), Region::Kind::Circle);
    const tiled::Object oval{.position = {0.0F, 0.0F}, .size = {40.0F, 20.0F}, .shape = tiled::Object::Shape::Ellipse};
    EXPECT_NEAR(Region::fromObject(oval).getArea(), math::Math::kPi * 200.0F, 3.0F);

    const tiled::Object triangle{.position = {100.0F, 100.0F}, .shape = tiled::Object::Shape::Polygon, .points = {{0.0F, 0.0F}, {10.0F, 0.0F}, {0.0F, 10.0F}}};
    EXPECT_NEAR(Region::fromObject(triangle).getArea(), 50.0F, 1e-3F);
    EXPECT_TRUE(Region::fromObject(triangle).contains({102.0F, 102.0F}));

    const tiled::Object point{.shape = tiled::Object::Shape::Point};
    EXPECT_THROW((void)Region::fromObject(point), std::invalid_argument);
}

TEST_F(Procedural2DTest, CavesAreDeterministicWithSolidBorders) {
    math::Random first(77);
    math::Random second(77);
    const spatial2d::CellGrid cave = CellularAutomaton::generate({.width = 48, .height = 32}, first);
    EXPECT_TRUE(std::ranges::equal(cave.getValues(), CellularAutomaton::generate({.width = 48, .height = 32}, second).getValues()));

    for (int x = 0; x < 48; ++x) {
        EXPECT_EQ((cave[{x, 0}]), CellularAutomaton::kWall);
        EXPECT_EQ((cave[{x, 31}]), CellularAutomaton::kWall);
    }
    const std::size_t floors = countValue(cave, CellularAutomaton::kFloor);
    EXPECT_GT(floors, 48U * 32U / 4U);
    EXPECT_LT(floors, 48U * 32U * 3U / 4U);

    // A lone wall with no wall neighbors disappears in one step.
    spatial2d::CellGrid lone(5, 5, CellularAutomaton::kFloor);
    lone.set({2, 2}, CellularAutomaton::kWall);
    EXPECT_EQ(CellularAutomaton::countWalls(lone, {2, 1}, false), 1);
    const spatial2d::CellGrid smoothed = CellularAutomaton::step(lone, {.solidBorder = false});
    EXPECT_EQ(countValue(smoothed, CellularAutomaton::kWall), 0U);
}

TEST_F(Procedural2DTest, DrunkardWalkOpensTheRequestedShare) {
    math::Random random(9);
    const spatial2d::CellGrid map = DrunkardWalk::generate({.width = 40, .height = 30, .coverage = 0.35F, .walkers = 3}, random);
    EXPECT_EQ(countValue(map, DrunkardWalk::kFloor), static_cast<std::size_t>(std::ceil(38.0F * 28.0F * 0.35F)));
    EXPECT_EQ(countFloorRegions(map), 1U);
    for (int y = 0; y < 30; ++y) {
        EXPECT_EQ((map[{0, y}]), DrunkardWalk::kWall);
        EXPECT_EQ((map[{39, y}]), DrunkardWalk::kWall);
    }
    EXPECT_THROW((void)DrunkardWalk::generate({.width = 2}, random), std::invalid_argument);
}

TEST_F(Procedural2DTest, DungeonsJoinEveryRoom) {
    for (const Dungeon::Method method : {Dungeon::Method::Bsp, Dungeon::Method::Placement}) {
        math::Random random(31);
        const Dungeon::Result dungeon = Dungeon::generate({.method = method, .width = 80, .height = 50}, random);
        ASSERT_GE(dungeon.rooms.size(), 4U);
        EXPECT_EQ(dungeon.connections.size(), dungeon.rooms.size() - 1);
        EXPECT_EQ(countFloorRegions(dungeon.grid), 1U);

        for (const Dungeon::Room& room : dungeon.rooms) {
            EXPECT_GE(room.x, 1);
            EXPECT_GE(room.y, 1);
            EXPECT_LE(room.x + room.width, 79);
            EXPECT_LE(room.y + room.height, 49);
            EXPECT_GE(room.width, 4);
            EXPECT_LE(room.width, 10);
            EXPECT_EQ((dungeon.grid[{room.getCenterX(), room.getCenterY()}]), Dungeon::kFloor);
        }
    }

    math::Random random(1);
    EXPECT_THROW((void)Dungeon::generate({.minimumRoomSize = 6, .maximumRoomSize = 4}, random), std::invalid_argument);
    EXPECT_THROW((void)Dungeon::generate({.minimumRoomSize = 9, .minimumLeafSize = 10}, random), std::invalid_argument);
}

TEST_F(Procedural2DTest, MazesArePerfect) {
    for (const Maze::Algorithm algorithm : {Maze::Algorithm::Backtracker, Maze::Algorithm::Prim, Maze::Algorithm::Kruskal}) {
        math::Random random(6);
        const Maze maze = Maze::generate(21, 13, algorithm, random);
        // A perfect maze is a spanning tree of its cells: one passage fewer than cells, and every cell reachable.
        EXPECT_EQ(maze.getPassageCount(), 21U * 13U - 1U);
        const spatial2d::CellGrid tiles = maze.toGrid();
        EXPECT_EQ(tiles.getWidth(), 43);
        EXPECT_EQ(tiles.getHeight(), 27);
        EXPECT_EQ(countFloorRegions(tiles), 1U);
        EXPECT_EQ(countValue(tiles, 0), 21U * 13U * 2U - 1U);
    }

    Maze maze(2, 1);
    maze.open(0, 0, Maze::kEast);
    EXPECT_EQ(maze.getOpenings(1, 0), Maze::kWest);
    EXPECT_THROW(maze.open(0, 0, Maze::kNorth), std::invalid_argument);
    EXPECT_THROW((void)maze.getOpenings(2, 0), std::out_of_range);
    EXPECT_EQ(Maze::algorithmFromName("kruskal"), Maze::Algorithm::Kruskal);
}

} // namespace haylen::procedural2d
