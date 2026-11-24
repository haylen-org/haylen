#include <gtest/gtest.h>

#include <vector>

#include "support/LuaFixture.hpp"

namespace haylen::lua::spatial2d {

TEST(GridAlgorithmsLuaTest, RunsGridAlgorithmsFromLua) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        spatial2d = require('haylen.spatial2d')
        grid = spatial2d.newCellGrid(8, 6)
        for row = 0, 5 do grid:set(5, row, 1) end
        grid:set(5, 2, 0)
        function cells(list)
            local parts = {}
            for _, cell in ipairs(list) do parts[#parts + 1] = cell.x .. ':' .. cell.y end
            return table.concat(parts, ' ')
        end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return grid.width .. 'x' .. grid.height .. ' ' .. grid:get(5, 0) .. grid:get(5, 2) .. ' ' .. tostring(grid:contains(8, 0))"), "8x6 10 false");
    EXPECT_EQ(fixture.lua("local hit = spatial2d.raycastGrid(grid, {8, 8}, {200, 8}, 16) return hit.column .. ':' .. hit.row .. ' ' .. hit.x .. ' ' .. hit.normalX"), "5:0 80.0 -1.0");
    EXPECT_EQ(fixture.lua("return tostring(spatial2d.raycastGrid(grid, {8, 40}, {200, 40}, {16, 16}))"), "nil");
    EXPECT_NE(fixture.lua("spatial2d.raycastGrid(grid, {8, 40}, {200, 40}, -16)").find("positive and finite size"), std::string::npos);
    EXPECT_EQ(fixture.lua("local crossed = spatial2d.traverseGrid({5, 5}, {35, 17}, 10) return cells(crossed) .. ' ' .. crossed[1].distance .. ' ' .. tostring(crossed[2].distance > 5)"), "0:0 1:0 1:1 2:1 3:1 0.0 true");
    EXPECT_EQ(fixture.lua("return cells(spatial2d.traverseGrid({5, 5}, {5, 25}, {10, 20}))"), "0:0 0:1");
    EXPECT_NE(fixture.lua("spatial2d.traverseGrid({0, 0}, {1000000, 0}, 1)").find("at most 65536 cells"), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.traverseGrid({0, 0}, {1e10, 0}, 1)").find("32-bit range of cells"), std::string::npos);
    EXPECT_EQ(fixture.lua("return cells(spatial2d.line(0, 0, 3, 1)) .. ' / ' .. #spatial2d.circle(0, 0, 3)"), "0:0 1:0 2:1 3:1 / 16");
    EXPECT_EQ(fixture.lua("local seen = {} for _, cell in ipairs(spatial2d.fieldOfView(grid, 2, 2, 10)) do seen[cell.x .. ':' .. cell.y] = true end return tostring(seen['7:2']) .. ' ' .. tostring(seen['7:0']) .. ' ' .. tostring(seen['5:0'])"), "true nil true");
    EXPECT_EQ(fixture.lua("return #spatial2d.fieldOfView(grid, 2, 2, 2000000000) == #spatial2d.fieldOfView(grid, 2, 2, 10)"), "true");
    EXPECT_EQ(fixture.lua("return #spatial2d.fieldOfView(grid, -3, 2, 2000000000) .. ' ' .. #spatial2d.fieldOfView(grid, 7, 5, 0)"), "0 1");
    EXPECT_EQ(fixture.lua("local fill = spatial2d.floodFill(grid, 0, 0) return #fill .. ' ' .. #spatial2d.floodFill(grid, 5, 0, {diagonal = true})"), "43 2");
    EXPECT_EQ(fixture.lua("spatial2d.floodFill(grid, 7, 5, {value = 3}) return grid:get(0, 0) .. grid:get(6, 0)"), "33");
    EXPECT_EQ(fixture.lua("local labels, count = spatial2d.components(grid, {background = 3}) return count .. ' ' .. labels:get(5, 0) .. labels:get(5, 3) .. labels:get(0, 0)"), "2 120");
    EXPECT_EQ(fixture.lua("local labels, count = spatial2d.components(grid) return count"), "3");
    EXPECT_EQ(fixture.lua("local outline = spatial2d.visibilityPolygon({50, 50}, {{{70, 40}, {70, 60}}}, {0, 0, 100, 100}) return tostring(#outline >= 6) .. ' ' .. tostring(math.abs(require('haylen.math').polygonSignedArea(outline)) < 10000)"), "true true");

    fixture.runLua("sets = spatial2d.newUnionFind(4)");
    EXPECT_EQ(fixture.lua("return tostring(sets:unite(1, 2)) .. tostring(sets:unite(2, 1)) .. ' ' .. sets:find(2) .. ' ' .. tostring(sets:connected(1, 3)) .. ' ' .. sets:setSize(1) .. ' ' .. sets.setCount .. ' ' .. sets:add() .. ' ' .. sets.size"), "truefalse 1 false 2 3 5 5");
    EXPECT_NE(fixture.lua("sets:find(0)").find("count from 1"), std::string::npos);
    EXPECT_NE(fixture.lua("sets:find(6)").find("Element 6 is outside the union-find of 5 elements."), std::string::npos);
    EXPECT_NE(fixture.lua("sets:unite(1, 7)").find("Element 7 is outside the union-find of 5 elements."), std::string::npos);
    EXPECT_EQ(fixture.lua("sets:reset(3) return sets.size .. ' ' .. sets.setCount .. ' ' .. tostring(sets:connected(1, 2))"), "3 3 false");
    EXPECT_NE(fixture.lua("grid:set(9, 0, 1)").find("outside the cell grid"), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.floodFill(grid, 0, 0, {fill = 2})").find("Unknown option \"fill\""), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.newCellGrid(0, 3)").find("positive size"), std::string::npos);
}

} // namespace haylen::lua::spatial2d
