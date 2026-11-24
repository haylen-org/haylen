#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "support/LuaFixture.hpp"

namespace haylen::lua::spatial2d {

using Ids = std::vector<std::uint64_t>;

TEST(Spatial2DLuaTest, PicksAndAimsFromScreenPoints) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        spatial2d = require('haylen.spatial2d')
        camera = require('haylen.graphics2d').newCamera()
        camera.position = {1000, 500}
        local screen = require('haylen.viewport').visibleRect()
        cx, cy = screen.x + screen.width / 2, screen.y + screen.height / 2
        wx, wy = camera:screenToWorld(cx + 50, cy)
        crate = {name = 'crate'}
        hash = spatial2d.newHashGrid(32)
        hash:set(crate, {wx - 5, wy - 5, 10, 10})
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("local x1, y1, x2, y2 = spatial2d.screenRay(camera, cx + 50, cy) local ox, oy = camera:screenToWorld(cx, cy) return tostring(x1 == ox and y1 == oy and math.abs(x2 - wx) < 1e-3 and math.abs(y2 - wy) < 1e-3)"), "true");
    EXPECT_EQ(fixture.lua("return #hash:pick(camera, cx + 50, cy) .. ' ' .. hash:pick(camera, cx + 50, cy)[1].name .. ' ' .. #hash:pick(camera, cx - 300, cy)"), "1 crate 0");
    EXPECT_EQ(fixture.lua("local tree = spatial2d.newAabbTree() tree:set(crate, {wx - 5, wy - 5, 10, 10}) return #tree:pick(camera, cx + 50, cy)"), "1");
}

TEST(Spatial2DLuaTest, StoresLuaValuesInEveryStructure) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        spatial2d = require('haylen.spatial2d')
        function names(values)
            local list = {}
            for _, value in ipairs(values) do list[#list + 1] = value.name end
            return table.concat(list, ',')
        end
        function hitNames(hits)
            local list = {}
            for _, hit in ipairs(hits) do list[#list + 1] = hit.value.name end
            return table.concat(list, ',')
        end
        function fill(structure)
            goblin = {name = 'goblin'}
            tree = {name = 'tree'}
            rock = {name = 'rock'}
            structure:set(goblin, {0, 0, 10, 10})
            structure:set(tree, {x = 40, y = 0, width = 20, height = 20})
            structure:set(rock, {200, 200, 8, 8})
            return structure
        end
        function check(structure)
            fill(structure)
            local results = {
                structure.size,
                names(structure:query({0, 0, 50, 50})),
                names(structure:queryCircle(204, 204, 1)),
                names(structure:queryPoint(5, 5)),
                hitNames(structure:raycast(-10, 5, 300, 5)),
                hitNames(structure:raycast(-10, 5, 300, 5, 1)),
                structure:raycast(-10, 5, 300, 5)[2].x,
                structure:nearest(0, 0, 500).name,
                structure:nearest(0, 0, 500, function(value) return value ~= goblin end).name,
                tostring(structure:nearest(300, 300, 5)),
                names(structure:kNearest(190, 190, 2)),
                names(structure:kNearest(190, 190, 5, 100)),
                tostring(structure:remove(tree)) .. tostring(structure:remove(tree)) .. tostring(structure:has(tree)),
            }

            structure:clear()
            results[#results + 1] = structure.size .. tostring(structure:has(rock))
            return table.concat(results, ' ')
        end
    )");
    // clang-format on

    const std::string expected = "3 goblin,tree rock goblin goblin,tree goblin 40.0 goblin tree nil rock,tree rock truefalsefalse 0false";
    EXPECT_EQ(fixture.lua("return check(spatial2d.newHashGrid(32))"), expected);
    EXPECT_EQ(fixture.lua("return check(spatial2d.newQuadTree({-500, -500, 1000, 1000}, {maxEntries = 1, maxDepth = 4}))"), expected);
    EXPECT_EQ(fixture.lua("return check(spatial2d.newAabbTree(2))"), expected);
    EXPECT_EQ(fixture.lua("local hash = fill(spatial2d.newHashGrid(16)) return hash.cellSize .. ' ' .. hash:bounds(tree).x .. ' ' .. tostring(hash:bounds({}))"), "16.0 40.0 nil");
    EXPECT_EQ(fixture.lua("local quad = fill(spatial2d.newQuadTree({0, 0, 400, 400}, {maxEntries = 1})) return quad.area.width .. ' ' .. quad.nodeCount"), "400.0 13");
    EXPECT_EQ(fixture.lua("local aabb = fill(spatial2d.newAabbTree()) return aabb.margin .. ' ' .. aabb.height"), "4.0 2");

    // clang-format off
    fixture.runLua(R"(
        points = spatial2d.newKdTree()
        points:set(goblin, 0, 0)
        points:set(tree, 50, 0, 10)
        points:set(rock, 200, 200)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return tostring(points.built)"), "false");
    EXPECT_NE(fixture.lua("points:kNearest(0, 0, 1)").find("needs \"build\""), std::string::npos);
    EXPECT_EQ(fixture.lua("points:build() return names(points:kNearest(45, 0, 2)) .. ' ' .. points:point(tree).x .. ' ' .. names(points:queryPoint(55, 0)) .. ' ' .. hitNames(points:raycast(-5, 0, 100, 0))"), "tree,goblin 50.0 tree goblin,tree");

    EXPECT_NE(fixture.lua("spatial2d.newHashGrid(32):set(nil, {0, 0, 1, 1})").find("a value to store is required"), std::string::npos);
    EXPECT_EQ(fixture.lua("local hash = spatial2d.newHashGrid(8) pcall(hash.set, hash, rock, {0, 0, -1, 1}) return hash:has(rock)"), "false");
    EXPECT_NE(fixture.lua("fill(spatial2d.newAabbTree()):nearest(0, 0, 10, function() error('filter failed') end)").find("filter failed"), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.newHashGrid(-2)").find("positive cell size"), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.newQuadTree({0, 0, 10, 10}, {depth = 3})").find("Unknown option \"depth\""), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.newQuadTree({0, 0, 10, 10}, {maxDepth = 40})").find("a depth from 0 to 16"), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.newHashGrid(1):set(rock, {0, 0, 1000, 1000})").find("at most 65536 cells"), std::string::npos);
    EXPECT_NE(fixture.lua("fill(spatial2d.newHashGrid(8)):nearest(0 / 0, 0, 10)").find("point must be finite"), std::string::npos);
}

TEST(Spatial2DLuaTest, NearestNeverOffersValuesThatAcceptRemoved) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        spatial2d = require('haylen.spatial2d')
        hash = spatial2d.newHashGrid(32)
        near, middle, far = {name = 'near'}, {name = 'middle'}, {name = 'far'}
        hash:set(near, {0, 0, 4, 4})
        hash:set(middle, {50, 0, 4, 4})
        hash:set(far, {100, 0, 4, 4})
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("local offered = {} local found = hash:nearest(0, 0, 500, function(value) offered[#offered + 1] = value.name if value == near then hash:remove(middle) end return value == far end) return found.name .. ' ' .. table.concat(offered, ',')"), "far near,far");
    EXPECT_EQ(fixture.lua("local offered = 0 local found = hash:nearest(0, 0, 500, function() offered = offered + 1 hash:clear() return false end) return tostring(found) .. ' ' .. offered .. ' ' .. hash.size"), "nil 1 0");
}

} // namespace haylen::lua::spatial2d
