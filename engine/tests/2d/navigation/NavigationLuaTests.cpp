#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "support/EngineFixture.hpp"

namespace haylen {

class NavigationLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // clang-format off
        fixture.runLua(R"(
            navigation2d = require('haylen.navigation2d')
            function cells(path)
                local list = {}
                for _, cell in ipairs(path) do list[#list + 1] = cell.x .. ':' .. cell.y end
                return table.concat(list, ' ')
            end
        )");
        // clang-format on
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    // Runs an asynchronous call in a task and waits until it stores its result in the global done.
    void await(const std::string& call) {
        fixture.runLua("done = nil require('async').spawn(function() done = {" + call + ":await()} end)");
        ASSERT_TRUE(fixture.frameUntil([this] { return lua("return done ~= nil") == "true"; }, std::chrono::seconds(10)));
    }

    test::EngineFixture fixture;
};

TEST_F(NavigationLuaTest, SearchesGridsOfEveryLayout) {
    // clang-format off
    fixture.runLua(R"(
        grid = navigation2d.newGrid(12, 8)
        for y = 0, 6 do grid:setWalkable(5, y, false) end
        hexes = navigation2d.newGrid(6, 6, {topology = 'hexagonal', staggerX = true, staggerEven = true})
    )");
    // clang-format on

    EXPECT_EQ(lua("return grid.topology .. ' ' .. hexes.topology .. ' ' .. tostring(hexes.staggerX) .. tostring(hexes.staggerEven) .. tostring(grid.staggerX)"), "square hexagonal truetruefalse");
    EXPECT_EQ(lua("local path, cost = grid:findPath(0, 7, 4, 7) return #path .. ' ' .. cost"), "5 4.0");
    EXPECT_EQ(lua("local _, a = grid:findPath(0, 0, 11, 0) local _, b = grid:findPath(0, 0, 11, 0, {jumpPoint = true}) return tostring(math.abs(a - b) < 1e-4)"), "true");
    EXPECT_EQ(lua("local _, a = grid:findPath(0, 0, 11, 0) local _, b = grid:findPath(0, 0, 11, 0, {heuristic = 'euclidean'}) return tostring(math.abs(a - b) < 1e-4)"), "true");
    EXPECT_EQ(lua("local _, a = grid:findPath(0, 0, 11, 0) local _, b = grid:findPath(0, 0, 11, 0, {weight = 2}) return tostring(b >= a - 1e-4 and b <= a * 2)"), "true");
    EXPECT_EQ(lua("local path, cost = hexes:findPath(0, 0, 4, 0) return #path .. ' ' .. cost"), "5 4.0");
    EXPECT_EQ(lua("grid:findPath(0, 0, 11, 0) local plain = grid.expandedCount grid:findPath(0, 0, 11, 0, {jumpPoint = true}) return tostring(grid.expandedCount > 0 and grid.expandedCount < plain)"), "true");
    EXPECT_EQ(lua("return grid:estimate(0, 0, 3, 4, 'manhattan') .. ' ' .. grid:estimate(0, 0, 3, 4, 'chebyshev') .. ' ' .. hexes:estimate(0, 0, 4, 0, 'octile')"), "7.0 4.0 4.0");

    EXPECT_EQ(lua("local hit = grid:raycast({8, 40}, {600, 40}, 32) return hit.column .. ' ' .. hit.row .. ' ' .. hit.x .. ' ' .. hit.normalX"), "5 1 160.0 -1.0");
    EXPECT_EQ(lua("return tostring(grid:raycast({8, 250}, {300, 250}, {32, 32}))"), "nil");

    EXPECT_EQ(lua("grid:setCost(0, 0, 3) return tostring(grid.uniformCost)"), "false");
    EXPECT_NE(lua("grid:findPath(0, 0, 11, 0, {jumpPoint = true})").find("every cell costs 1"), std::string::npos);
    EXPECT_EQ(lua("grid:findPathAsync(0, 0, 11, 0, {jumpPoint = true})"), "error: test:1: Jump point search needs a grid where every cell costs 1.");
    EXPECT_EQ(lua("grid:findPathAsync(0, 0, 11, 0, {weight = 0.5})"), "error: test:1: A search weight must be finite and at least 1.");
    EXPECT_NE(lua("hexes:findPath(0, 0, 4, 0, {heuristic = 'octile'})").find("take no heuristic"), std::string::npos);
    EXPECT_NE(lua("grid:findPath(0, 0, 11, 0, {weight = 0.5})").find("at least 1"), std::string::npos);
    EXPECT_NE(lua("grid:findPath(0, 0, 11, 0, {heuristic = 'taxicab'})").find("unknown value 'taxicab'"), std::string::npos);
    EXPECT_NE(lua("navigation2d.newGrid(4, 4, {topology = 'triangle'})").find("unknown value 'triangle'"), std::string::npos);
    EXPECT_NE(lua("hexes:raycast({0, 0}, {10, 0}, 8)").find("need a square grid"), std::string::npos);

    await("grid:findPathAsync(0, 7, 4, 7)");
    EXPECT_EQ(lua("return cells(done[1])"), "0:7 1:7 2:7 3:7 4:7");
    await("grid:findPathAsync(0, 0, 5, 0)");
    EXPECT_EQ(lua("return tostring(done[1])"), "nil");

    // Background searches share one copy of the grid, which every change replaces.
    fixture.runLua("grid:setWalkable(2, 7, false)");
    await("grid:findPathAsync(0, 7, 4, 7)");
    EXPECT_EQ(lua("return tostring(cells(done[1]):find('2:7') == nil) .. ' ' .. #done[1]"), "true 5");
}

TEST_F(NavigationLuaTest, BuildsDijkstraMapsFlowFieldsAndHierarchies) {
    fixture.runLua("corridor = navigation2d.newGrid(10, 1)");

    EXPECT_EQ(lua("local map = corridor:dijkstraMap({{0, 0}}) local x, y = map:next(3, 0) return map:value(3, 0) .. ' ' .. x .. ':' .. y .. ' ' .. tostring(map:next(0, 0)) .. ' ' .. map.width .. 'x' .. map.height"), "3.0 2:0 nil 10x1");
    EXPECT_EQ(lua("local map = corridor:dijkstraMap({{0, 0, value = 5}, {x = 9, y = 0}}) return map:value(0, 0) .. ' ' .. map:value(4, 0)"), "5.0 5.0");
    EXPECT_EQ(lua("local values = corridor:dijkstraMap({{0, 0}}):values() return #values .. ' ' .. values[1] .. ' ' .. values[10]"), "10 0.0 9.0");
    EXPECT_NE(lua("corridor:dijkstraMap({{4294967296, 0}})").find("integer out of range"), std::string::npos);
    EXPECT_EQ(lua("local map = corridor:dijkstraMap({{0, 0}}) map:flee() local x = map:next(1, 0) return x"), "2");
    EXPECT_EQ(lua("corridor:setWalkable(5, 0, false) local map = corridor:dijkstraMap({{0, 0}}) return tostring(map:value(5, 0) == math.huge) .. ' ' .. tostring(map:value(7, 0) == math.huge)"), "true true");
    EXPECT_NE(lua("corridor:dijkstraMap({{0, 0}}):flee(0.5)").find("negative coefficient"), std::string::npos);
    EXPECT_NE(lua("corridor:dijkstraMap({{0, 0}}):value(20, 0)").find("outside the Dijkstra map"), std::string::npos);
    fixture.runLua("corridor:setWalkable(5, 0, true)");

    EXPECT_EQ(lua("local field = corridor:flowField({{9, 0}}) local x, y = field:next(0, 0) return x .. ':' .. y .. ' ' .. field:direction(0, 0).x .. ' ' .. field:distance(0, 0) .. ' ' .. tostring(field:next(9, 0)) .. ' ' .. field.width"), "1:0 1.0 9.0 nil 10");
    EXPECT_NE(lua("corridor:flowField({{9, 0}}):distance(0, 3)").find("outside the flow field"), std::string::npos);

    // clang-format off
    fixture.runLua(R"(
        big = navigation2d.newGrid(64, 64)
        for y = 0, 63 do big:setWalkable(32, y, y == 60) end
        router = big:hierarchical({clusterSize = 16})
    )");
    // clang-format on
    EXPECT_EQ(lua("local path, cost = router:findPath(0, 0, 63, 0) local _, best = big:findPath(0, 0, 63, 0) return path[1].x .. ':' .. path[#path].x .. ' ' .. tostring(cost >= best - 1e-3 and cost <= best * 1.25 + 2)"), "0:63 true");
    EXPECT_EQ(lua("return router.clusterSize .. ' ' .. tostring(router.nodeCount > 0) .. ' ' .. tostring(router.diagonal) .. ' ' .. tostring(big:hierarchical({diagonal = false}).diagonal)"), "16 true true false");
    EXPECT_EQ(lua("big:setWalkable(32, 60, false) router:update(32, 60) return tostring(router:findPath(0, 0, 63, 0))"), "nil");
    EXPECT_EQ(lua("big:setWalkable(32, 10, true) router:update(32, 0, 32, 63) return tostring(router:findPath(0, 0, 63, 0) ~= nil)"), "true");
    EXPECT_EQ(lua("router:rebuild() return tostring(router:findPath(0, 0, 63, 0) ~= nil)"), "true");
    // A cluster larger than the grid covers all of it, with search buffers no larger than the grid.
    EXPECT_EQ(lua("local whole = big:hierarchical({clusterSize = 2147483647}) return whole.clusterSize .. ' ' .. whole.nodeCount .. ' ' .. tostring(whole:findPath(0, 0, 63, 0) ~= nil)"), "2147483647 0 true");
    EXPECT_NE(lua("big:hierarchical({clusterSize = 1})").find("at least 2 cells"), std::string::npos);
    EXPECT_EQ(lua("navigation2d.newGrid(8, 8, {topology = 'staggered'}):hierarchical()"), "error: test:1: Hierarchical path finding needs a square grid.");

    EXPECT_EQ(lua("corridor:dijkstraMapAsync({{0, 0, value = math.huge}})"), "error: test:1: A Dijkstra map source needs a finite value.");
    await("corridor:dijkstraMapAsync({{0, 0}})");
    EXPECT_EQ(lua("return done[1]:value(9, 0) .. ' ' .. done[1]:next(9, 0)"), "9.0 8");
    await("corridor:flowFieldAsync({{9, 0}}, {diagonal = false})");
    EXPECT_EQ(lua("return done[1]:distance(0, 0)"), "9.0");

    // A hierarchy built in the background from a copy of the grid reads the grid itself once it arrives.
    fixture.runLua("pending = big:hierarchicalAsync({clusterSize = 16})");
    await("pending");
    EXPECT_EQ(lua("background = done[1] return background.clusterSize .. ' ' .. tostring(background.nodeCount == router.nodeCount) .. ' ' .. tostring(background:findPath(0, 0, 63, 0) ~= nil)"), "16 true true");

    // Every coroutine that awaits the promise gets a path finder of its own.
    await("pending");
    EXPECT_EQ(lua("return tostring(rawequal(done[1], background)) .. ' ' .. tostring(done[1].nodeCount == background.nodeCount)"), "false true");
    EXPECT_EQ(lua("return tostring(done[1]:findPath(0, 0, 63, 0) ~= nil)"), "true");

    EXPECT_EQ(lua("big:setWalkable(32, 10, false) background:update(32, 10) return tostring(background:findPath(0, 0, 63, 0))"), "nil");
    EXPECT_EQ(lua("navigation2d.newGrid(8, 8, {topology = 'staggered'}):hierarchicalAsync()"), "error: test:1: Hierarchical path finding needs a square grid.");
    EXPECT_EQ(lua("big:hierarchicalAsync({clusterSize = 1})"), "error: test:1: Hierarchical path finding needs clusters of at least 2 cells.");
    EXPECT_NE(lua("big:hierarchicalAsync({size = 8})").find("Unknown option 'size'"), std::string::npos);
}

TEST_F(NavigationLuaTest, FindsGraphPathsAroundDisabledPoints) {
    // clang-format off
    fixture.runLua(R"(
        roads = navigation2d.newGraph()
        roads:addPoint(1, 0, 0)
        roads:addPoint(2, 100, 0)
        roads:addPoint(3, 100, 100)
        roads:addPoint(4, 0, 100, 3)
        roads:connect(1, 2)
        roads:connect(2, 3)
        roads:connect(1, 4)
        roads:connect(4, 3)
    )");
    // clang-format on

    EXPECT_EQ(lua("local route, cost = roads:findPath(1, 3) return table.concat(route, ' ') .. ' ' .. cost"), "1 2 3 200.0");
    EXPECT_EQ(lua("roads:setEnabled(2, false) local route, cost = roads:findPath(1, 3) return table.concat(route, ' ') .. ' ' .. cost .. ' ' .. tostring(roads:enabled(2))"), "1 4 3 400.0 false");
    EXPECT_EQ(lua("return roads:closest(90, 10) .. ' ' .. roads:closest(90, 10, true)"), "1 2");
    EXPECT_EQ(lua("local costs = roads:distances(1) return costs[4] .. ' ' .. costs[3] .. ' ' .. tostring(costs[2])"), "300.0 400.0 nil");
    EXPECT_EQ(lua("return table.concat(roads:neighbors(1), ' ') .. ' ' .. table.concat(roads:points(), ' ') .. ' ' .. roads.size"), "2 4 1 2 3 4 4");
    EXPECT_EQ(lua("roads:disconnect(1, 2) roads:connect(1, 2, false) return tostring(roads:connected(1, 2)) .. tostring(roads:connected(2, 1))"), "truefalse");
    EXPECT_EQ(lua("roads:setPosition(4, 0, 50) roads:setWeight(4, 2) return roads:position(4).y .. ' ' .. roads:weight(4)"), "50.0 2.0");
    EXPECT_EQ(lua("return tostring(roads:removePoint(4)) .. tostring(roads:removePoint(4)) .. tostring(roads:hasPoint(4)) .. ' ' .. tostring(roads:findPath(1, 3))"), "truefalsefalse nil");
    EXPECT_EQ(lua("roads:clear() return roads.size .. ' ' .. tostring(roads:closest(0, 0))"), "0 nil");
    EXPECT_EQ(lua("local far = navigation2d.newGraph() far:addPoint(7, 3e38, 0) far:addPoint(3, 3e38, 1) return far:closest(-3e38, 0)"), "3");

    EXPECT_NE(lua("roads:addPoint(1, 0, 0, 0.5)").find("at least 1"), std::string::npos);
    EXPECT_NE(lua("roads:addPoint(1, 0, 0) roads:connect(1, 1)").find("cannot connect to itself"), std::string::npos);
    EXPECT_NE(lua("roads:position(99)").find("Point 99 is not in the graph."), std::string::npos);
}

TEST_F(NavigationLuaTest, RoutesThroughNavigationMeshes) {
    // clang-format off
    fixture.runLua(R"(
        mesh = navigation2d.newNavMesh({{0, 0}, {400, 0}, {400, 300}, {0, 300}})
        block = mesh:addObstacle({{150, 50}, {250, 50}, {250, 250}, {150, 250}})
    )");
    // clang-format on

    EXPECT_EQ(lua("return tostring(mesh.dirty) .. ' ' .. #mesh.boundary .. ' ' .. mesh.obstacleCount"), "true 4 1");
    EXPECT_EQ(lua("local path, length = mesh:findPath(50, 150, 350, 150) return #path .. ' ' .. string.format('%.2f', length) .. ' ' .. tostring(mesh.dirty)"), "4 382.84 false");
    EXPECT_EQ(lua("local _, thin = mesh:findPath(50, 150, 350, 150) local _, wide = mesh:findPath(50, 150, 350, 150, 10) return tostring(wide > thin)"), "true");
    EXPECT_EQ(lua("return tostring(mesh:findPath(50, 150, 350, 150, 60))"), "nil");
    EXPECT_EQ(lua("return tostring(mesh:contains(200, 150)) .. tostring(mesh:contains(50, 150)) .. ' ' .. tostring(mesh:findTriangle(-5, -5)) .. ' ' .. math.type(mesh:findTriangle(10, 10))"), "falsetrue nil integer");
    EXPECT_EQ(lua("local x, y = mesh:closestPoint(-50, 150) return x .. ' ' .. y"), "0.0 150.0");
    EXPECT_EQ(lua("local triangles = mesh:triangles() return tostring(#triangles == mesh.triangleCount) .. ' ' .. #triangles[1] .. ' ' .. tostring(triangles[1][1].x ~= nil)"), "true 3 true");

    EXPECT_EQ(lua("mesh:setObstacle(block, {{150, -10}, {250, -10}, {250, 250}, {150, 250}}) return tostring(mesh.dirty) .. ' ' .. #mesh:findPath(50, 150, 350, 150)"), "true 4");
    EXPECT_EQ(lua("mesh:removeObstacle(block) local path, length = mesh:findPath(50, 150, 350, 150) return tostring(mesh:removeObstacle(block)) .. ' ' .. #path .. ' ' .. length"), "false 2 300.0");
    EXPECT_EQ(lua("mesh:addObstacle({{10, 10}, {20, 10}, {20, 20}}) mesh:clearObstacles() mesh:build() return mesh.obstacleCount .. ' ' .. tostring(mesh.dirty)"), "0 false");

    EXPECT_EQ(lua("local later = navigation2d.newNavMesh() later:setBoundary({{0, 0}, {100, 0}, {100, 100}, {0, 100}}) return #later.boundary .. ' ' .. tostring(later.dirty) .. ' ' .. tostring(later:contains(50, 50))"), "4 true true");
    EXPECT_EQ(lua("mesh:setBoundary({{0, 0}, {1, 1}})"), "error: test:1: A navigation mesh polygon needs at least three points.");
    EXPECT_NE(lua("navigation2d.newNavMesh():build()").find("needs a boundary"), std::string::npos);
    EXPECT_NE(lua("mesh:addObstacle({{0, 0}, {1, 1}})").find("at least three points"), std::string::npos);
    EXPECT_NE(lua("mesh:setObstacle(99, {{0, 0}, {1, 0}, {0, 1}})").find("Obstacle 99 is not in the navigation mesh."), std::string::npos);
    EXPECT_NE(lua("mesh:findPath(0, 0, 1, 1, -1)").find("not negative"), std::string::npos);

    await("navigation2d.buildNavMeshAsync({{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {{{40, 40}, {60, 40}, {60, 60}, {40, 60}}})");
    EXPECT_EQ(lua("return tostring(done[1].dirty) .. ' ' .. done[1].obstacleCount .. ' ' .. tostring(done[1].triangleCount > 0) .. ' ' .. tostring(done[1]:contains(50, 50))"), "false 1 true false");
}

TEST_F(NavigationLuaTest, MovesCrowdsWithoutOverlaps) {
    // clang-format off
    fixture.runLua(R"(
        crowd = navigation2d.newCrowd({separation = 0.25})
        a = crowd:addAgent({x = 0, y = 0, radius = 5, maxSpeed = 50})
        b = crowd:addAgent({x = 200, y = 4, radius = 5, maxSpeed = 50})
        crowd:setTarget(a, 200, 0)
        crowd:setTarget(b, 0, 4)
        closest = math.huge
        for _ = 1, 300 do
            crowd:step(1 / 30)
            closest = math.min(closest, (crowd:position(a) - crowd:position(b)):length())
        end
    )");
    // clang-format on

    EXPECT_EQ(lua("return tostring(closest >= 9.5) .. ' ' .. tostring((crowd:position(a) - crowd:target(a)):length() < 2) .. ' ' .. tostring((crowd:position(b) - crowd:target(b)):length() < 2)"), "true true true");
    EXPECT_EQ(lua("return crowd.agentCount .. ' ' .. crowd:radius(a) .. ' ' .. crowd:maxSpeed(a) .. ' ' .. crowd.separation .. ' ' .. crowd.alignment"), "2 5.0 50.0 0.25 0.0");
    EXPECT_EQ(lua("crowd.alignment = 0.5 crowd.cohesion = 0.25 return crowd.alignment .. ' ' .. crowd.cohesion"), "0.5 0.25");

    EXPECT_EQ(lua("crowd:clearTarget(a) crowd:step(1) crowd:step(1) return tostring(crowd:target(a)) .. ' ' .. crowd:velocity(a):length()"), "nil 0.0");
    EXPECT_EQ(lua("crowd:setPreferredVelocity(a, 0, 20) crowd:step(0.5) return crowd:velocity(a).y .. ' ' .. crowd:preferredVelocity(a).y .. ' ' .. tostring(crowd:target(a))"), "20.0 20.0 nil");
    EXPECT_EQ(lua("crowd:setPosition(a, 500, 500) return crowd:position(a).x"), "500.0");

    // A wall between an agent and its target makes it walk around the wall instead of through it.
    // clang-format off
    fixture.runLua(R"(
        crowd.separation, crowd.alignment, crowd.cohesion = 0, 0, 0
        crowd:addObstacle({{-50, 100}, {50, 100}})
        crowd:setPosition(a, 0, 60)
        crowd:setTarget(a, 0, 140)
        crossed = false
        for _ = 1, 120 do
            local before = crowd:position(a)
            crowd:step(1 / 30)
            local after = crowd:position(a)
            if before.y < 100 and after.y >= 100 and math.abs(after.x) < 50 then crossed = true end
        end
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(crossed)"), "false");
    EXPECT_EQ(lua("crowd:clearObstacles() return tostring(crowd:removeAgent(b)) .. tostring(crowd:removeAgent(b)) .. tostring(crowd:hasAgent(b)) .. ' ' .. crowd.agentCount"), "truefalsefalse 1");

    EXPECT_NE(lua("crowd:addObstacle({{0, 0}})").find("at least two points"), std::string::npos);
    EXPECT_NE(lua("crowd:step(0)").find("positive and finite time"), std::string::npos);
    EXPECT_NE(lua("crowd:position(99)").find("Agent 99 is not in the crowd."), std::string::npos);
    EXPECT_NE(lua("crowd:addAgent({speed = 1})").find("Unknown option 'speed'"), std::string::npos);
    EXPECT_NE(lua("crowd:addAgent({radius = -1})").find("positive time horizons"), std::string::npos);

    // Values that are not finite never reach the step, and targets far across the float range still pull at full speed.
    EXPECT_EQ(lua("crowd:addAgent({vx = 0 / 0})"), "error: test:1: A crowd agent needs a finite position, velocity, radius, speed and neighbor distance and positive time horizons.");
    EXPECT_EQ(lua("crowd:addObstacle({{0, 0}, {math.huge, 0}})"), "error: test:1: A crowd obstacle needs finite points.");
    EXPECT_EQ(lua("crowd:setPosition(a, 0 / 0, 0)"), "error: test:1: Crowd positions, targets and velocities must be finite.");
    EXPECT_EQ(lua("crowd:setTarget(a, math.huge, 0)"), "error: test:1: Crowd positions, targets and velocities must be finite.");
    EXPECT_EQ(lua("crowd:setPreferredVelocity(a, 0, -math.huge)"), "error: test:1: Crowd positions, targets and velocities must be finite.");
    EXPECT_EQ(lua("crowd:setPosition(a, -3e38, 0) crowd:setTarget(a, 3e38, 0) crowd:step(0.5) return crowd:velocity(a).x"), "50.0");
}

TEST_F(NavigationLuaTest, SteersWithFlockingAndAvoidance) {
    fixture.runLua("agent = navigation2d.newAgent({maxSpeed = 100})");

    EXPECT_EQ(lua("local force = agent:alignment({{0, 10}, {0, 30}}) return force.x .. ' ' .. force.y"), "0.0 100.0");
    EXPECT_EQ(lua("local force = agent:cohesion({{10, 0}, {30, 0}}) return force.x .. ' ' .. force.y"), "100.0 0.0");
    EXPECT_EQ(lua("return agent:alignment({}):length() .. ' ' .. agent:cohesion({}):length()"), "0.0 0.0");

    EXPECT_EQ(lua("return agent:avoid({{center = {50, 5}, radius = 10}}, 100):length()"), "0.0");
    EXPECT_EQ(lua("agent.velocity = {100, 0} local force = agent:avoid({{center = {50, 5}, radius = 10}}, 100) return tostring(force.x == 0 and force.y < -50 and force.y > -60)"), "true");
    EXPECT_EQ(lua("return agent:avoid({{{50, 50}, 10}}, 100):length() .. ' ' .. agent:avoid({{center = {150, 0}, radius = 10}}, 100):length()"), "0.0 0.0");
}

} // namespace haylen
