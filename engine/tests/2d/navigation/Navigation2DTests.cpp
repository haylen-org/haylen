#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include <vector>

#include "haylen/2d/navigation/Grid.hpp"
#include "haylen/2d/navigation/GridSearch.hpp"
#include "haylen/2d/navigation/SteeringAgent.hpp"
#include "haylen/2d/navigation/Wanderer.hpp"
#include "support/EngineFixture.hpp"

namespace haylen {

namespace {

using Cell = navigation2d::Grid::Cell;
using Path = std::vector<Cell>;

class GridTest : public ::testing::Test {
  protected:
    // Checks that every step moves to a walkable neighbor and that diagonal steps never cut a blocked corner.
    static void expectWalkable(const navigation2d::Grid& grid, const Path& path) {
        for (std::size_t index = 1; index < path.size(); ++index) {
            const Cell from = path[index - 1];
            const Cell to = path[index];
            EXPECT_TRUE(grid.isWalkable(to));
            EXPECT_LE(std::abs(to.x - from.x), 1);
            EXPECT_LE(std::abs(to.y - from.y), 1);
            if (to.x != from.x && to.y != from.y) {
                EXPECT_TRUE(grid.isWalkable({to.x, from.y}) && grid.isWalkable({from.x, to.y}));
            }
        }
    }

    [[nodiscard]] static bool visits(const Path& path, Cell cell) {
        return std::ranges::find(path, cell) != path.end();
    }

    [[nodiscard]] static Path findPath(const navigation2d::Grid& grid, Cell start, Cell goal, const navigation2d::GridSearch::Options& options = {}) {
        navigation2d::GridSearch search;
        const std::span<const Cell> path = search.findPath(grid, start, goal, options);
        return {path.begin(), path.end()};
    }
};

} // namespace

TEST_F(GridTest, FindsShortestPathsWithAndWithoutDiagonals) {
    navigation2d::Grid grid(6, 6);
    EXPECT_EQ(findPath(grid, {0, 0}, {4, 0}, {.diagonal = false}), (Path{{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}}));
    EXPECT_EQ(findPath(grid, {0, 0}, {3, 3}), (Path{{0, 0}, {1, 1}, {2, 2}, {3, 3}}));
    EXPECT_EQ(findPath(grid, {0, 0}, {3, 3}, {.diagonal = false}).size(), 7U);
    EXPECT_EQ(findPath(grid, {2, 2}, {2, 2}), (Path{{2, 2}}));
    EXPECT_EQ(findPath(grid, {0, 0}, {9, 0}), Path{});
}

TEST_F(GridTest, RoutesAroundWallsCostsAndCorners) {
    navigation2d::Grid grid(7, 5);
    for (int y = 0; y < 4; ++y) {
        grid.setWalkable({3, y}, false);
    }
    const Path around = findPath(grid, {0, 0}, {6, 0});
    ASSERT_FALSE(around.empty());
    EXPECT_EQ(around.front(), (Cell{0, 0}));
    EXPECT_EQ(around.back(), (Cell{6, 0}));
    EXPECT_TRUE(visits(around, {3, 4}));
    expectWalkable(grid, around);

    // A blocked corner turns a single diagonal step into two straight ones.
    navigation2d::Grid corner(3, 3);
    corner.setWalkable({1, 0}, false);
    EXPECT_EQ(findPath(corner, {0, 0}, {1, 1}), (Path{{0, 0}, {0, 1}, {1, 1}}));

    navigation2d::Grid swamp(5, 3);
    for (int x = 1; x <= 3; ++x) {
        swamp.setCost({x, 1}, 10.0F);
    }
    const Path dry = findPath(swamp, {0, 1}, {4, 1});
    EXPECT_FALSE(visits(dry, {2, 1}));
    expectWalkable(swamp, dry);
    EXPECT_EQ(swamp.getCost({2, 1}), 10.0F);
    EXPECT_EQ(swamp.getCost({2, 0}), 1.0F);
}

TEST_F(GridTest, ReportsMissingPaths) {
    navigation2d::Grid grid(5, 5);
    for (const Cell wall : {Cell{1, 0}, Cell{1, 1}, Cell{0, 1}}) {
        grid.setWalkable(wall, false);
    }
    EXPECT_EQ(findPath(grid, {4, 4}, {0, 0}), Path{});
    EXPECT_EQ(findPath(grid, {1, 1}, {4, 4}), Path{});
    EXPECT_EQ(findPath(grid, {4, 4}, {1, 1}), Path{});
    EXPECT_EQ(findPath(grid, {-1, 0}, {4, 4}), Path{});
    EXPECT_FALSE(grid.isWalkable({5, 0}));
    EXPECT_FALSE(grid.contains({0, 5}));
    EXPECT_TRUE(grid.contains({4, 4}));
}

TEST_F(GridTest, TracesLinesAndSmoothsPaths) {
    navigation2d::Grid grid(8, 8);
    EXPECT_TRUE(grid.hasLineOfSight({0, 0}, {7, 3}));
    EXPECT_TRUE(grid.hasLineOfSight({7, 7}, {0, 0}));
    EXPECT_TRUE(grid.hasLineOfSight({3, 3}, {3, 3}));

    grid.setWalkable({3, 1}, false);
    EXPECT_FALSE(grid.hasLineOfSight({0, 1}, {7, 1}));
    EXPECT_FALSE(grid.hasLineOfSight({0, 0}, {6, 2}));
    EXPECT_TRUE(grid.hasLineOfSight({0, 0}, {7, 0}));

    // The segment from 0,0 to 2,2 passes exactly through the corner shared with 1,0 and 0,1.
    navigation2d::Grid corner(3, 3);
    EXPECT_TRUE(corner.hasLineOfSight({0, 0}, {2, 2}));
    corner.setWalkable({1, 0}, false);
    EXPECT_FALSE(corner.hasLineOfSight({0, 0}, {2, 2}));
    EXPECT_FALSE(corner.hasLineOfSight({1, 0}, {1, 2}));

    navigation2d::Grid walls(9, 5);
    for (int y = 0; y < 4; ++y) {
        walls.setWalkable({4, y}, false);
    }
    const Path path = findPath(walls, {0, 0}, {8, 0});
    Path smooth = path;
    walls.smoothPath(smooth);
    EXPECT_LT(smooth.size(), path.size());
    EXPECT_EQ(smooth.front(), path.front());
    EXPECT_EQ(smooth.back(), path.back());
    for (std::size_t index = 1; index < smooth.size(); ++index) {
        EXPECT_TRUE(walls.hasLineOfSight(smooth[index - 1], smooth[index]));
    }
    EXPECT_EQ(findPath(walls, {0, 0}, {8, 0}, {.smooth = true}), smooth);
    Path pair{{1, 1}, {2, 2}};
    walls.smoothPath(pair);
    EXPECT_EQ(pair, (Path{{1, 1}, {2, 2}}));
}

TEST_F(GridTest, RejectsInvalidInput) {
    EXPECT_THROW(navigation2d::Grid(0, 4), std::invalid_argument);
    EXPECT_THROW(navigation2d::Grid(70000, 70000), std::invalid_argument);

    navigation2d::Grid grid(4, 4);
    EXPECT_THROW(grid.setWalkable({4, 0}, false), std::out_of_range);
    EXPECT_THROW(grid.setCost({0, 0}, 0.5F), std::invalid_argument);
    EXPECT_THROW(grid.setCost({0, 0}, std::nanf("")), std::invalid_argument);
    EXPECT_THROW((void)grid.getCost({-1, 0}), std::out_of_range);
}

TEST(SteeringTest, SeeksFleesArrivesAndSeparates) {
    const navigation2d::SteeringAgent agent{.position = {0.0F, 0.0F}, .velocity = {}, .maxSpeed = 100.0F, .maxForce = 50.0F};
    EXPECT_EQ(agent.seek({10.0F, 0.0F}), math::Vec2(100.0F, 0.0F));
    EXPECT_EQ(agent.flee({10.0F, 0.0F}), math::Vec2(-100.0F, 0.0F));
    EXPECT_EQ(agent.arrive({50.0F, 0.0F}, 100.0F), math::Vec2(50.0F, 0.0F));
    EXPECT_EQ(agent.arrive({500.0F, 0.0F}, 100.0F), math::Vec2(100.0F, 0.0F));
    EXPECT_EQ(agent.arrive({50.0F, 0.0F}, 0.0F), math::Vec2(100.0F, 0.0F));

    const navigation2d::SteeringAgent moving{.position = {3.0F, 4.0F}, .velocity = {10.0F, 0.0F}, .maxSpeed = 100.0F, .maxForce = 50.0F};
    EXPECT_EQ(moving.arrive({3.0F, 4.0F}, 10.0F), math::Vec2(-10.0F, 0.0F));
    EXPECT_EQ(moving.seek({3.0F, 4.0F}), math::Vec2(-10.0F, 0.0F));

    const std::vector<math::Vec2> neighbors{{5.0F, 0.0F}, {0.0F, 0.0F}, {40.0F, 0.0F}};
    EXPECT_EQ(agent.separation(neighbors, 10.0F), math::Vec2(-50.0F, 0.0F));
    EXPECT_EQ(agent.separation({}, 10.0F), math::Vec2());
}

TEST(SteeringTest, AppliesLimitedForcesAndReachesTargets) {
    navigation2d::SteeringAgent agent{.position = {}, .velocity = {}, .maxSpeed = 10.0F, .maxForce = 20.0F};
    agent.apply({100.0F, 0.0F}, 0.5F);
    EXPECT_EQ(agent.velocity, math::Vec2(10.0F, 0.0F));
    EXPECT_EQ(agent.position, math::Vec2(5.0F, 0.0F));

    navigation2d::SteeringAgent walker{.position = {0.0F, 0.0F}, .velocity = {}, .maxSpeed = 80.0F, .maxForce = 400.0F};
    for (int step = 0; step < 600; ++step) {
        walker.apply(walker.arrive({200.0F, -100.0F}, 60.0F), 1.0F / 60.0F);
    }
    EXPECT_NEAR(walker.position.x, 200.0F, 1.0F);
    EXPECT_NEAR(walker.position.y, -100.0F, 1.0F);
    EXPECT_LT(walker.velocity.getLength(), 1.0F);
}

TEST(SteeringTest, WandersDeterministicallyPerSeed) {
    const navigation2d::SteeringAgent agent{.position = {}, .velocity = {20.0F, 0.0F}, .maxSpeed = 50.0F, .maxForce = 100.0F};
    navigation2d::Wanderer first(7);
    navigation2d::Wanderer second(7);
    navigation2d::Wanderer other(8, {.distance = 40.0F, .radius = 20.0F, .jitter = 6.0F});
    bool diverged = false;
    for (int step = 0; step < 30; ++step) {
        const math::Vec2 force = first.steer(agent, 0.1F);
        EXPECT_EQ(force, second.steer(agent, 0.1F));
        EXPECT_LE(force.getLength(), agent.maxSpeed + agent.velocity.getLength() + 0.001F);
        diverged = diverged || force != other.steer(agent, 0.1F);
    }
    EXPECT_TRUE(diverged);
    EXPECT_EQ(other.getSettings().distance, 40.0F);
    other.setSettings({.distance = 1.0F});
    EXPECT_EQ(other.getSettings().distance, 1.0F);
}

TEST(Navigation2DLuaTest, FindsPathsAndSteersAgents) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        navigation2d = require('haylen.navigation2d')
        grid = navigation2d.newGrid(7, 5)
        for y = 0, 3 do grid:setWalkable(3, y, false) end
        function cells(path)
            local list = {}
            for _, cell in ipairs(path) do list[#list + 1] = cell.x .. ':' .. cell.y end
            return table.concat(list, ' ')
        end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return grid.width .. 'x' .. grid.height .. ' ' .. tostring(grid:walkable(3, 0)) .. tostring(grid:contains(6, 4)) .. tostring(grid:contains(7, 0))"), "7x5 falsetruefalse");
    EXPECT_EQ(fixture.lua("return cells(grid:findPath(0, 4, 6, 4))"), "0:4 1:4 2:4 3:4 4:4 5:4 6:4");
    EXPECT_EQ(fixture.lua("return cells(grid:findPath(0, 0, 2, 0, {diagonal = false}))"), "0:0 1:0 2:0");
    EXPECT_EQ(fixture.lua("local path = grid:findPath(0, 0, 6, 0, {smooth = true}) return path[1].x .. ':' .. path[#path].x .. ' ' .. tostring(#path < 9)"), "0:6 true");
    EXPECT_EQ(fixture.lua("return cells(grid:smoothPath({{0, 4}, {1, 4}, {2, 4}}))"), "0:4 2:4");
    EXPECT_EQ(fixture.lua("return tostring(grid:lineOfSight(0, 0, 6, 0)) .. tostring(grid:lineOfSight(0, 4, 6, 4))"), "falsetrue");
    EXPECT_EQ(fixture.lua("grid:setWalkable(3, 4, false) return tostring(grid:findPath(0, 0, 6, 0))"), "nil");
    EXPECT_EQ(fixture.lua("grid:setCost(1, 1, 4) return grid:cost(1, 1) .. ' ' .. grid:cost(0, 0)"), "4.0 1.0");

    // clang-format off
    fixture.runLua(R"(
        agent = navigation2d.newAgent({x = 0, y = 0, maxSpeed = 100, maxForce = 50, seed = 3, wanderJitter = 2})
        twin = navigation2d.newAgent({x = 0, y = 0, maxSpeed = 100, maxForce = 50, seed = 3, wanderJitter = 2})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("local force = agent:seek({10, 0}) return force.x .. ',' .. force.y"), "100.0,0.0");
    EXPECT_EQ(fixture.lua("return agent:flee(navigation2d.newAgent({x = 5}).position).x .. ' ' .. agent:arrive({50, 0}, 100).x"), "-100.0 50.0");
    EXPECT_EQ(fixture.lua("return agent:separation({{5, 0}, {0, 0}}, 10).x"), "-50.0");
    EXPECT_EQ(fixture.lua("agent:apply({100, 0}, 1) return agent.position.x .. ' ' .. agent.velocity.x"), "50.0 50.0");
    EXPECT_EQ(fixture.lua("agent.position = {0, 0} agent.velocity = {0, 0} agent.maxSpeed = 20 return agent.maxSpeed .. ' ' .. agent.maxForce"), "20.0 50.0");
    EXPECT_EQ(fixture.lua("twin.maxSpeed = 20 local a = agent:wander(0.1) local b = twin:wander(0.1) return a.x == b.x and a.y == b.y"), "true");
    EXPECT_EQ(fixture.lua("return agent.wanderDistance .. ' ' .. agent.wanderRadius .. ' ' .. agent.wanderJitter"), "60.0 30.0 2.0");

    // Changing the wander settings of one twin makes it steer differently from the other.
    EXPECT_EQ(fixture.lua("twin.wanderDistance = 10 twin.wanderRadius = 80 twin.wanderJitter = 0 return twin.wanderDistance .. ' ' .. twin.wanderRadius .. ' ' .. twin.wanderJitter"), "10.0 80.0 0.0");
    EXPECT_EQ(fixture.lua("local a = agent:wander(0.1) local b = twin:wander(0.1) return a.x == b.x and a.y == b.y"), "false");
    EXPECT_NE(fixture.lua("twin.wanderRadius = 'wide'").find("number expected"), std::string::npos);

    EXPECT_NE(fixture.lua("grid:findPath(0, 0, 1, 1, {cheap = true})").find("Unknown option \"cheap\""), std::string::npos);
    EXPECT_NE(fixture.lua("grid:setWalkable(9, 9, true)").find("outside the navigation grid"), std::string::npos);
    EXPECT_NE(fixture.lua("grid:setCost(0, 0, 0)").find("at least 1"), std::string::npos);
    EXPECT_NE(fixture.lua("navigation2d.newAgent({speed = 3})").find("Unknown option \"speed\""), std::string::npos);
    EXPECT_NE(fixture.lua("grid:smoothPath({{x = 'a'}})").find("error: "), std::string::npos);
}

} // namespace haylen
