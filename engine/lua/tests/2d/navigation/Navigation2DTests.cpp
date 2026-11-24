#include <gtest/gtest.h>

#include "support/LuaFixture.hpp"

namespace haylen::lua::navigation2d {

TEST(Navigation2DLuaTest, FindsPathsAndSteersAgents) {
    test::LuaFixture fixture;
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

} // namespace haylen::lua::navigation2d
