#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include "haylen/2d/navigation/DijkstraMap.hpp"
#include "haylen/2d/navigation/FlowField.hpp"
#include "haylen/2d/navigation/Graph.hpp"
#include "haylen/2d/navigation/GraphSearch.hpp"
#include "haylen/2d/navigation/Grid.hpp"
#include "haylen/2d/navigation/GridSearch.hpp"
#include "haylen/2d/navigation/HierarchicalPathfinder.hpp"
#include "haylen/math/Random.hpp"

namespace haylen {

namespace {

using Cell = navigation2d::Grid::Cell;
using Path = std::vector<Cell>;

navigation2d::Grid randomGrid(int width, int height, float wallChance, std::uint64_t seed, const navigation2d::Grid::Layout& layout = {}) {
    navigation2d::Grid grid(width, height, layout);
    math::Random random(seed);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            grid.setWalkable({x, y}, !random.chance(wallChance));
        }
    }
    return grid;
}

// Adds up a path the way searches price it: every step costs its length times the cost of the cell it enters.
float pathCost(const navigation2d::Grid& grid, std::span<const Cell> path, bool diagonal) {
    float cost = 0.0F;
    for (std::size_t index = 1; index < path.size(); ++index) {
        float length = std::numeric_limits<float>::infinity();
        grid.forEachStep(path[index - 1], diagonal, [&](Cell next, float step) { length = next == path[index] ? step : length; });
        cost += length * grid.getCost(path[index]);
    }
    return cost;
}

} // namespace

TEST(GridSearchTest, FindsCheapestPathsWithEveryAdmissibleHeuristic) {
    navigation2d::Grid grid = randomGrid(40, 30, 0.25F, 3);
    grid.setCost({10, 10}, 5.0F);
    navigation2d::DijkstraMap exact;
    navigation2d::GridSearch search;
    math::Random random(9);
    int found = 0;
    for (int probe = 0; probe < 60; ++probe) {
        const Cell start{random.range(0, 39), random.range(0, 29)};
        const Cell goal{random.range(0, 39), random.range(0, 29)};
        const std::vector<navigation2d::DijkstraMap::Source> sources{{.cell = goal}};
        exact.compute(grid, sources);
        const float cheapest = grid.isWalkable(start) ? exact.getValue(start) : std::numeric_limits<float>::infinity();

        for (const navigation2d::Grid::Heuristic heuristic : {navigation2d::Grid::Heuristic::Octile, navigation2d::Grid::Heuristic::Euclidean, navigation2d::Grid::Heuristic::Chebyshev}) {
            const std::span<const Cell> path = search.findPath(grid, start, goal, {.heuristic = heuristic});
            if (!std::isfinite(cheapest)) {
                EXPECT_TRUE(path.empty());
                continue;
            }
            ASSERT_FALSE(path.empty());
            EXPECT_NEAR(search.getCost(), cheapest, 1e-3F);
            EXPECT_NEAR(pathCost(grid, path, true), cheapest, 1e-3F);
        }
        found += std::isfinite(cheapest) ? 1 : 0;
    }
    EXPECT_GT(found, 20);
}

TEST(GridSearchTest, TradesLengthForSpeedWithWeights) {
    const navigation2d::Grid grid = randomGrid(80, 80, 0.2F, 5);
    navigation2d::GridSearch search;
    const std::span<const Cell> exact = search.findPath(grid, {0, 0}, {79, 79});
    ASSERT_FALSE(exact.empty());
    const float cheapest = search.getCost();
    const std::size_t exactWork = search.getExpandedCount();

    const std::span<const Cell> fast = search.findPath(grid, {0, 0}, {79, 79}, {.weight = 2.0F});
    ASSERT_FALSE(fast.empty());
    EXPECT_LE(search.getCost(), cheapest * 2.0F);
    EXPECT_GE(search.getCost(), cheapest - 1e-3F);
    EXPECT_LT(search.getExpandedCount(), exactWork);

    // Manhattan overestimates diagonal walks, so it finds a path but not always the cheapest one.
    EXPECT_FALSE(search.findPath(grid, {0, 0}, {79, 79}, {.heuristic = navigation2d::Grid::Heuristic::Manhattan}).empty());
    EXPECT_THROW((void)search.findPath(grid, {0, 0}, {1, 1}, {.weight = 0.5F}), std::invalid_argument);
}

TEST(GridSearchTest, JumpPointSearchMatchesAStar) {
    navigation2d::GridSearch astar;
    navigation2d::GridSearch jumps;
    std::size_t astarWork = 0;
    std::size_t jumpWork = 0;
    for (const bool diagonal : {true, false}) {
        for (const navigation2d::Grid::Topology topology : {navigation2d::Grid::Topology::Square, navigation2d::Grid::Topology::Staggered}) {
            const navigation2d::Grid grid = randomGrid(48, 48, 0.3F, diagonal ? 11 : 12, {.topology = topology});
            math::Random random(21);
            for (int probe = 0; probe < 80; ++probe) {
                const Cell start{random.range(0, 47), random.range(0, 47)};
                const Cell goal{random.range(0, 47), random.range(0, 47)};
                const std::span<const Cell> expected = astar.findPath(grid, start, goal, {.diagonal = diagonal});
                const std::span<const Cell> path = jumps.findPath(grid, start, goal, {.diagonal = diagonal, .jumpPoint = true});
                ASSERT_EQ(path.empty(), expected.empty());
                if (path.empty()) {
                    continue;
                }
                EXPECT_NEAR(jumps.getCost(), astar.getCost(), 1e-3F);
                EXPECT_NEAR(pathCost(grid, path, diagonal), astar.getCost(), 1e-3F);
                EXPECT_EQ(path.front(), start);
                EXPECT_EQ(path.back(), goal);
                astarWork += astar.getExpandedCount();
                jumpWork += jumps.getExpandedCount();
            }
        }
    }
    EXPECT_LT(jumpWork, astarWork);

    navigation2d::Grid costly(4, 4);
    costly.setCost({1, 1}, 2.0F);
    EXPECT_FALSE(costly.hasUniformCost());
    EXPECT_THROW((void)jumps.findPath(costly, {0, 0}, {3, 3}, {.jumpPoint = true}), std::invalid_argument);
    costly.setCost({1, 1}, 1.0F);
    EXPECT_TRUE(costly.hasUniformCost());
}

TEST(GridSearchTest, WalksHexagonalGrids) {
    for (const bool staggerX : {false, true}) {
        for (const bool staggerEven : {false, true}) {
            navigation2d::Grid grid(9, 9, {.topology = navigation2d::Grid::Topology::Hexagonal, .staggerX = staggerX, .staggerEven = staggerEven});
            int neighbors = 0;
            float lengths = 0.0F;
            // clang-format off
            grid.forEachStep({4, 4}, false, [&](Cell, float length) {
                lengths += length;
                ++neighbors;
            });
            // clang-format on
            EXPECT_EQ(neighbors, 6);
            EXPECT_EQ(lengths, 6.0F);

            // Every step of a path joins two neighbors, and open ground costs exactly the hex distance.
            navigation2d::GridSearch search;
            const std::span<const Cell> path = search.findPath(grid, {0, 0}, {8, 6});
            ASSERT_FALSE(path.empty());
            EXPECT_EQ(search.getCost(), grid.estimate({0, 0}, {8, 6}, navigation2d::Grid::Heuristic::Octile));
            EXPECT_EQ(static_cast<float>(path.size() - 1), search.getCost());
            for (std::size_t index = 1; index < path.size(); ++index) {
                bool adjacent = false;
                grid.forEachStep(path[index - 1], false, [&](Cell next, float) { adjacent = adjacent || next == path[index]; });
                EXPECT_TRUE(adjacent);
            }
            EXPECT_TRUE(grid.hasLineOfSight({0, 0}, {8, 6}));
            for (int row = 0; row < 8; ++row) {
                grid.setWalkable({4, row}, false);
            }
            EXPECT_FALSE(grid.hasLineOfSight({0, 3}, {8, 3}));
            EXPECT_GT(search.findPath(grid, {0, 3}, {8, 3}).size(), 9U);
            EXPECT_THROW((void)search.findPath(grid, {0, 0}, {1, 1}, {.jumpPoint = true}), std::invalid_argument);
            EXPECT_THROW((void)search.findPath(grid, {0, 0}, {1, 1}, {.heuristic = navigation2d::Grid::Heuristic::Octile}), std::invalid_argument);
        }
    }
}

TEST(GridSearchTest, WalksStaggeredGridsAsTurnedSquares) {
    for (const bool staggerX : {false, true}) {
        for (const bool staggerEven : {false, true}) {
            const navigation2d::Grid grid(12, 12, {.topology = navigation2d::Grid::Topology::Staggered, .staggerX = staggerX, .staggerEven = staggerEven});
            for (int y = 0; y < 12; ++y) {
                for (int x = 0; x < 12; ++x) {
                    EXPECT_EQ(grid.fromLattice(grid.toLattice({x, y})), (Cell{x, y}));
                }
            }

            int sides = 0;
            int corners = 0;
            grid.forEachStep({5, 5}, true, [&](Cell, float length) { (length == 1.0F ? sides : corners) += 1; });
            EXPECT_EQ(sides, 4);
            EXPECT_EQ(corners, 4);

            navigation2d::GridSearch search;
            const std::span<const Cell> path = search.findPath(grid, {1, 1}, {10, 9});
            ASSERT_FALSE(path.empty());
            EXPECT_NEAR(search.getCost(), grid.estimate({1, 1}, {10, 9}, navigation2d::Grid::Heuristic::Octile), 1e-4F);
            EXPECT_TRUE(grid.hasLineOfSight({1, 1}, {1, 1}));
        }
    }
}

TEST(DijkstraMapTest, LeadsDownhillToTheClosestSourceAndAwayWhenFleeing) {
    navigation2d::Grid grid(20, 10);
    for (int y = 0; y < 8; ++y) {
        grid.setWalkable({10, y}, false);
    }
    navigation2d::DijkstraMap map;
    const std::vector<navigation2d::DijkstraMap::Source> sources{{.cell = {2, 2}}, {.cell = {17, 2}, .value = 3.0F}, {.cell = {10, 0}}};
    map.compute(grid, sources);
    EXPECT_EQ(map.getValue({2, 2}), 0.0F);
    EXPECT_EQ(map.getValue({17, 2}), 3.0F);
    EXPECT_EQ(map.getValue({10, 0}), std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(map.getValue({5, 2}), 3.0F);
    EXPECT_FALSE(map.getNext(grid, {2, 2}).has_value());

    // Walking downhill from any cell ends on a source along a cheapest path.
    Cell walker{18, 8};
    float spent = 0.0F;
    for (std::optional<Cell> next = map.getNext(grid, walker); next; next = map.getNext(grid, walker)) {
        spent += pathCost(grid, std::vector<Cell>{walker, *next}, true);
        walker = *next;
    }
    EXPECT_EQ(walker, (Cell{17, 2}));
    EXPECT_NEAR(spent + 3.0F, map.getValue({18, 8}), 1e-4F);

    // A flee map leads away from the sources, so a walker ends farther than it began.
    map.compute(grid, std::vector<navigation2d::DijkstraMap::Source>{{.cell = {5, 5}}});
    map.flee(grid);
    Cell runner{6, 5};
    for (int step = 0; step < 40; ++step) {
        if (const std::optional<Cell> next = map.getNext(grid, runner)) {
            runner = *next;
        }
    }
    EXPECT_GT(std::abs(runner.x - 5) + std::abs(runner.y - 5), 8);
    EXPECT_THROW(map.flee(grid, 0.5F), std::invalid_argument);
    EXPECT_THROW(map.flee(navigation2d::Grid(3, 3), -1.0F), std::invalid_argument);
    EXPECT_THROW((void)map.getValue({20, 0}), std::out_of_range);
}

TEST(FlowFieldTest, PointsEveryCellAlongACheapestPathToAGoal) {
    const navigation2d::Grid grid = randomGrid(30, 30, 0.25F, 17);
    const std::vector<Cell> goals{{3, 3}, {26, 20}};
    navigation2d::FlowField field;
    field.compute(grid, goals);
    EXPECT_EQ(field.getWidth(), 30);
    EXPECT_EQ(field.getHeight(), 30);

    navigation2d::GridSearch search;
    for (int y = 0; y < 30; y += 3) {
        for (int x = 0; x < 30; x += 3) {
            const Cell cell{x, y};
            float cheapest = std::numeric_limits<float>::infinity();
            for (const Cell goal : goals) {
                if (!search.findPath(grid, cell, goal).empty()) {
                    cheapest = std::min(cheapest, search.getCost());
                }
            }
            if (std::isfinite(cheapest)) {
                EXPECT_NEAR(field.getDistance(cell), cheapest, 1e-3F);
            } else {
                EXPECT_EQ(field.getDistance(cell), std::numeric_limits<float>::infinity());
            }

            // Following the steps reaches a goal and spends exactly the distance.
            Cell walker = cell;
            float spent = 0.0F;
            for (std::optional<Cell> next = field.getNext(walker); next; next = field.getNext(walker)) {
                const math::Vec2 direction = field.getDirection(walker);
                EXPECT_NEAR(direction.getLength(), 1.0F, 1e-5F);
                spent += pathCost(grid, std::vector<Cell>{walker, *next}, true);
                walker = *next;
            }
            if (std::isfinite(cheapest)) {
                EXPECT_TRUE(walker == goals[0] || walker == goals[1]);
                EXPECT_NEAR(spent, cheapest, 1e-3F);
            } else {
                EXPECT_TRUE(field.getDirection(cell).isZero());
            }
        }
    }
    EXPECT_THROW((void)field.getNext({30, 0}), std::out_of_range);
}

TEST(HierarchicalPathfinderTest, FindsNearOptimalPathsAndFollowsLocalChanges) {
    navigation2d::Grid grid = randomGrid(96, 96, 0.22F, 31);
    navigation2d::HierarchicalPathfinder hierarchy(grid, {.clusterSize = 12});
    EXPECT_GT(hierarchy.getNodeCount(), 0U);
    navigation2d::GridSearch search;
    math::Random random(8);

    // clang-format off
    const auto compare = [&](int probes) {
        for (int probe = 0; probe < probes; ++probe) {
            const Cell start{random.range(0, 95), random.range(0, 95)};
            const Cell goal{random.range(0, 95), random.range(0, 95)};
            const std::span<const Cell> expected = search.findPath(grid, start, goal);
            const std::span<const Cell> path = hierarchy.findPath(grid, start, goal);
            ASSERT_EQ(path.empty(), expected.empty()) << start.x << "," << start.y << " to " << goal.x << "," << goal.y;
            if (path.empty()) {
                continue;
            }
            EXPECT_EQ(path.front(), start);
            EXPECT_EQ(path.back(), goal);
            EXPECT_NEAR(pathCost(grid, path, true), hierarchy.getCost(), 1e-2F);
            EXPECT_GE(hierarchy.getCost(), search.getCost() - 1e-3F);
            EXPECT_LE(hierarchy.getCost(), search.getCost() * 1.25F + 2.0F);
        }
    };
    // clang-format on
    compare(80);

    // A wall across the middle only rebuilds the clusters it touches.
    for (int x = 0; x < 90; ++x) {
        grid.setWalkable({x, 48}, false);
    }
    hierarchy.update(grid, {0, 48}, {89, 48});
    compare(60);
    EXPECT_TRUE(hierarchy.findPath(grid, {0, 0}, {0, 0}).size() == 1 || !grid.isWalkable({0, 0}));

    EXPECT_THROW(navigation2d::HierarchicalPathfinder(navigation2d::Grid(8, 8, {.topology = navigation2d::Grid::Topology::Hexagonal})), std::invalid_argument);
    EXPECT_THROW(navigation2d::HierarchicalPathfinder(grid, {.clusterSize = 1}), std::invalid_argument);
    EXPECT_THROW((void)hierarchy.findPath(navigation2d::Grid(8, 8), {0, 0}, {1, 1}), std::invalid_argument);
}

TEST(HierarchicalPathfinderTest, FindsNoPathThroughCellsThatChangedWithoutAnUpdate) {
    // Three clusters in a row, joined by one entrance on each border at row 2.
    navigation2d::Grid grid(12, 4);
    navigation2d::HierarchicalPathfinder hierarchy(grid, {.clusterSize = 4});
    ASSERT_FALSE(hierarchy.findPath(grid, {0, 1}, {11, 1}).empty());

    // A wall splits the middle cluster, whose stale entrances still claim a way through.
    for (int y = 0; y < 4; ++y) {
        grid.setWalkable({5, y}, false);
    }
    EXPECT_TRUE(hierarchy.findPath(grid, {0, 1}, {11, 1}).empty());
    EXPECT_TRUE(std::isinf(hierarchy.getCost()));

    // A blocked entrance cell is never stepped on either.
    for (int y = 0; y < 4; ++y) {
        grid.setWalkable({5, y}, true);
    }
    grid.setWalkable({4, 2}, false);
    EXPECT_TRUE(hierarchy.findPath(grid, {0, 1}, {11, 1}).empty());

    hierarchy.update(grid, {4, 2}, {4, 2});
    const std::span<const Cell> path = hierarchy.findPath(grid, {0, 1}, {11, 1});
    ASSERT_FALSE(path.empty());
    EXPECT_TRUE(std::ranges::all_of(path, [&grid](Cell cell) { return grid.isWalkable(cell); }));
}

TEST(GraphTest, FindsPathsThroughEnabledWaypoints) {
    navigation2d::Graph graph;
    graph.addPoint(1, {0.0F, 0.0F});
    graph.addPoint(2, {100.0F, 0.0F});
    graph.addPoint(3, {100.0F, 100.0F});
    graph.addPoint(4, {0.0F, 100.0F}, 3.0F);
    graph.addPoint(5, {50.0F, 50.0F});
    graph.connect(1, 2);
    graph.connect(2, 3);
    graph.connect(1, 4);
    graph.connect(4, 3);
    graph.connect(1, 5, false);
    graph.connect(5, 3, false);

    navigation2d::GraphSearch search;
    const std::span<const std::int64_t> shortcut = search.findPath(graph, 1, 3);
    EXPECT_EQ(std::vector<std::int64_t>(shortcut.begin(), shortcut.end()), (std::vector<std::int64_t>{1, 5, 3}));
    EXPECT_NEAR(search.getCost(), 100.0F * std::sqrt(2.0F), 1e-3F);

    // One-way links only lead forward, and disabled points are walked around.
    const std::span<const std::int64_t> back = search.findPath(graph, 3, 1);
    EXPECT_EQ(std::vector<std::int64_t>(back.begin(), back.end()), (std::vector<std::int64_t>{3, 2, 1}));
    graph.setEnabled(5, false);
    const std::span<const std::int64_t> around = search.findPath(graph, 1, 3);
    EXPECT_EQ(std::vector<std::int64_t>(around.begin(), around.end()), (std::vector<std::int64_t>{1, 2, 3}));
    EXPECT_TRUE(search.findPath(graph, 1, 5).empty());

    // A heavy point is avoided until it is the only way.
    graph.disconnect(2, 3);
    EXPECT_FALSE(graph.isConnected(2, 3));
    const std::span<const std::int64_t> heavy = search.findPath(graph, 1, 3);
    EXPECT_EQ(std::vector<std::int64_t>(heavy.begin(), heavy.end()), (std::vector<std::int64_t>{1, 4, 3}));
    EXPECT_NEAR(search.getCost(), 400.0F, 1e-3F);

    search.computeDistances(graph, 1);
    EXPECT_NEAR(search.getDistance(graph, 3), 400.0F, 1e-3F);
    EXPECT_NEAR(search.getDistance(graph, 2), 100.0F, 1e-3F);
    EXPECT_EQ(search.getDistance(graph, 5), std::numeric_limits<float>::infinity());

    std::vector<std::int64_t> neighbors;
    graph.getNeighbors(1, neighbors);
    EXPECT_EQ(neighbors, (std::vector<std::int64_t>{2, 4, 5}));
    EXPECT_EQ(graph.getClosestPoint({60.0F, 60.0F}), 3);
    EXPECT_EQ(graph.getClosestPoint({60.0F, 60.0F}, true), 5);

    EXPECT_TRUE(graph.removePoint(4));
    EXPECT_FALSE(graph.removePoint(4));
    EXPECT_TRUE(search.findPath(graph, 1, 3).empty());
    graph.addPoint(4, {0.0F, 100.0F});
    EXPECT_FALSE(graph.isConnected(1, 4));
    EXPECT_EQ(graph.size(), 5U);
    EXPECT_EQ(graph.getWeight(4), 1.0F);
    graph.setPosition(4, {5.0F, 5.0F});
    EXPECT_EQ(graph.getPosition(4), (math::Vec2{5.0F, 5.0F}));

    EXPECT_THROW(graph.connect(1, 99), std::out_of_range);
    EXPECT_THROW(graph.connect(1, 1), std::invalid_argument);
    EXPECT_THROW(graph.addPoint(6, {}, 0.5F), std::invalid_argument);
    EXPECT_THROW((void)search.findPath(graph, 1, 42), std::out_of_range);
    graph.clear();
    EXPECT_EQ(graph.size(), 0U);
    EXPECT_FALSE(graph.getClosestPoint({}).has_value());
}

} // namespace haylen
