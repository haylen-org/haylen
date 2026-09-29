#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "support/EngineFixture.hpp"

namespace haylen {

class Procedural2DLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("procedural = require('haylen.procedural2d') spatial = require('haylen.spatial2d') m = require('haylen.math')");
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

TEST_F(Procedural2DLuaTest, RegionsDescribeAreas) {
    EXPECT_EQ(lua("local r = procedural.region({0, 0, 100, 50}) return r.kind .. ' ' .. r.area .. ' ' .. r.bounds.width"), "rect 5000.0 100.0");
    EXPECT_EQ(lua("local r = procedural.region(m.rect(0, 0, 10, 10)) return r.kind .. ' ' .. tostring(r:contains({5, 5}))"), "rect true");
    EXPECT_EQ(lua("local r = procedural.region({center = {0, 0}, radius = 10, innerRadius = 5}) return r.kind .. ' ' .. tostring(r:contains({7, 0})) .. ' ' .. tostring(r:contains({2, 0}))"), "ring true false");
    EXPECT_EQ(lua("local r = procedural.region({center = {0, 0}, radius = 10}) return r.kind"), "circle");
    EXPECT_EQ(lua("local r = procedural.region({polygon = {{0, 0}, {10, 0}, {0, 10}}}) return r.kind .. ' ' .. r.area"), "polygon 50.0");
    EXPECT_EQ(lua("local r = procedural.region({shape = 'polygon', x = 100, y = 100, rotation = 0, points = {{x = 0, y = 0}, {x = 10, y = 0}, {x = 0, y = 10}}}) return tostring(r:contains({102, 102}))"), "true");
    EXPECT_EQ(lua("local r = procedural.region({shape = 'ellipse', x = 0, y = 0, width = 20, height = 20}) return r.kind"), "circle");
    EXPECT_EQ(lua("local r = procedural.region({0, 0, 10, 10}) local p = r:randomPoint(m.random(3)) return tostring(r:contains(p))"), "true");
    EXPECT_NE(lua("procedural.region({shape = 'point', x = 0, y = 0})").find("Only rectangle, ellipse and polygon"), std::string::npos);
    EXPECT_NE(lua("procedural.region({center = {0, 0}, radius = 1, color = 'red'})").find("Unknown option 'color'"), std::string::npos);
}

TEST_F(Procedural2DLuaTest, ScattersPointsWithTypesAndMaps) {
    EXPECT_EQ(lua("local points = procedural.scatter({region = {0, 0, 1000, 500}, density = 0.002, seed = 1}) return tostring(math.abs(#points - 1000) <= 1)"), "true");
    EXPECT_EQ(lua("local points = procedural.scatter({region = {0, 0, 400, 400}, method = 'grid', spacing = 40, jitter = 0}) return #points .. ' ' .. points[1].x .. ' ' .. points[1].type"), "100 20.0 1");
    // clang-format off
    fixture.runLua(R"(
        trees = procedural.scatter({
            region = {0, 0, 400, 400},
            method = 'poisson',
            spacing = 20,
            exclude = {{center = {200, 200}, radius = 60}},
            weights = {1, 3},
            random = m.random(8),
        })
        near = 0
        pines = 0
        for _, tree in ipairs(trees) do
            if m.vec2(tree.x, tree.y):distance({200, 200}) < 60 then near = near + 1 end
            if tree.type == 2 then pines = pines + 1 end
        end
        biomes = procedural.scatter({
            region = {0, 0, 400, 400},
            density = 0.01,
            seed = 2,
            biome = {seed = 4, frequency = 0.01},
            densityMap = function(point) return point.x < 200 and 1 or 0 end,
            layers = {{minimum = -1, maximum = 0, weights = {1, 0}}, {minimum = 0, maximum = 1, weights = {0, 1}}},
        })
        right = 0
        for _, point in ipairs(biomes) do
            if point.x >= 200 then right = right + 1 end
        end
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(#trees > 100) .. ' ' .. near .. ' ' .. tostring(pines > (#trees - pines))"), "true 0 true");
    EXPECT_EQ(lua("return tostring(#biomes > 0) .. ' ' .. right"), "true 0");
    EXPECT_EQ(lua("local a = procedural.scatter({region = {0, 0, 100, 100}, density = 0.01, seed = 5}) local b = procedural.scatter({region = {0, 0, 100, 100}, density = 0.01, seed = 5}) return a[3].x == b[3].x"), "true");
    EXPECT_NE(lua("procedural.scatter({region = {0, 0, 10, 10}, method = 'hex'})").find("unknown value 'hex'"), std::string::npos);
    EXPECT_NE(lua("procedural.scatter({region = {0, 0, 10, 10}, amount = 3})").find("Unknown option 'amount'"), std::string::npos);

    await("procedural.scatterAsync({region = {0, 0, 100, 100}, density = 0.01, seed = 5, densityMap = {seed = 1}})");
    EXPECT_EQ(lua("return tostring(#done[1] > 0)"), "true");
    EXPECT_NE(lua("procedural.scatterAsync({region = {0, 0, 10, 10}, densityMap = function() return 1 end})").find("noise table"), std::string::npos);
}

TEST_F(Procedural2DLuaTest, GeneratesMapsAsCellGrids) {
    fixture.runLua("cave = procedural.caves({width = 40, height = 30, seed = 3})");
    EXPECT_EQ(lua("return cave.width .. ' ' .. cave.height .. ' ' .. cave:get(0, 0)"), "40 30 1");
    EXPECT_EQ(lua("local walk = procedural.drunkardWalk({width = 20, height = 20, coverage = 0.3, seed = 2}) return walk:get(10, 10)"), "0");
    // clang-format off
    fixture.runLua(R"(
        dungeon = procedural.dungeon({method = 'placement', width = 60, height = 40, seed = 6})
        first = dungeon.rooms[1]
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(#dungeon.rooms >= 3) .. ' ' .. tostring(#dungeon.connections == #dungeon.rooms - 1) .. ' ' .. dungeon.grid:get(first.x, first.y)"), "true true 0");
    EXPECT_EQ(lua("return dungeon.connections[1][1] >= 1"), "true");

    fixture.runLua("maze = procedural.maze({width = 8, height = 6, algorithm = 'kruskal', seed = 1})");
    EXPECT_EQ(lua("return maze.width .. ' ' .. maze.height .. ' ' .. maze.passageCount .. ' ' .. maze:toGrid().width"), "8 6 47 17");
    EXPECT_EQ(lua("local open = procedural.maze({width = 2, height = 1}) return open:openings(0, 0) == procedural.east"), "true");
    EXPECT_EQ(lua("local closed = procedural.maze({width = 1, height = 1}) return closed:openings(0, 0)"), "0");
    EXPECT_NE(lua("procedural.maze({width = 2, height = 2, algorithm = 'eller'})").find("unknown value 'eller'"), std::string::npos);
    EXPECT_NE(lua("procedural.caves({size = 3})").find("Unknown option 'size'"), std::string::npos);

    await("procedural.cavesAsync({width = 20, height = 20, seed = 3})");
    EXPECT_EQ(lua("return done[1].width"), "20");
    await("procedural.dungeonAsync({width = 50, height = 40, seed = 1})");
    EXPECT_EQ(lua("return tostring(#done[1].rooms > 0)"), "true");
    await("procedural.mazeAsync({width = 5, height = 5, seed = 1})");
    EXPECT_EQ(lua("return done[1].passageCount"), "24");
    await("procedural.drunkardWalkAsync({width = 20, height = 20, seed = 4})");
    EXPECT_EQ(lua("return done[1]:get(10, 10)"), "0");
}

TEST_F(Procedural2DLuaTest, CollapsesWavesFromRulesAndSamples) {
    // clang-format off
    fixture.runLua(R"(
        coast = {{0, 0, 'right'}, {0, 0, 'down'}, {1, 1, 'right'}, {1, 1, 'down'}, {2, 2, 'right'}, {2, 2, 'down'}, {0, 1, 'right'}, {1, 0, 'right'}, {0, 1, 'down'}, {1, 0, 'down'}, {1, 2, 'right'}, {2, 1, 'right'}, {1, 2, 'down'}, {2, 1, 'down'}}
        tiles = procedural.waveFunctionCollapse({
            tiles = 3,
            allow = coast,
            weights = {1, 2, 1},
            width = 20,
            height = 15,
            seed = 5,
        })
        valid = true
        for y = 0, 14 do
            for x = 0, 18 do
                local a, b = tiles:get(x, y), tiles:get(x + 1, y)
                if math.abs(a - b) > 1 then valid = false end
            end
        end
        sample = spatial.newCellGrid(4, 1)
        for x = 0, 3 do sample:set(x, 0, x % 2) end
        stripes = procedural.waveFunctionCollapse({sample = sample, periodicSample = true, width = 8, height = 3, periodic = true, seed = 1})
        fixed = spatial.newCellGrid(20, 15, -1)
        fixed:set(0, 0, 0)
        fixed:set(1, 0, 2)
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(valid) .. ' ' .. tiles.width"), "true 20");
    EXPECT_EQ(lua("return tostring(stripes:get(0, 0) ~= stripes:get(1, 0)) .. ' ' .. tostring(stripes:get(0, 0) == stripes:get(2, 0))"), "true true");
    // Land fixed right next to sea can never work.
    EXPECT_EQ(lua("return tostring(procedural.waveFunctionCollapse({tiles = 3, allow = coast, width = 20, height = 15, fixed = fixed}))"), "nil");
    EXPECT_NE(lua("return procedural.waveFunctionCollapse({tiles = 3, allow = coast, width = 3, height = 1, fixed = fixed})").find("must match the size"), std::string::npos);
    EXPECT_NE(lua("return procedural.waveFunctionCollapse({tiles = 2, allow = {{0, 1, 'diagonal'}}})").find("unknown value 'diagonal'"), std::string::npos);

    await("procedural.waveFunctionCollapseAsync({sample = sample, periodicSample = true, width = 6, height = 2, periodic = true, seed = 2})");
    EXPECT_EQ(lua("return done[1].width"), "6");
}

TEST_F(Procedural2DLuaTest, TriangulatesAndRelaxesPoints) {
    fixture.runLua("points = {{0, 0}, {100, 0}, {0, 100}, {100, 100}, {50, 40}}");
    EXPECT_EQ(lua("local d = procedural.delaunay(points) return #d.triangles .. ' ' .. #d.hull .. ' ' .. #d.halfedges .. ' ' .. #d.neighbors[5]"), "12 4 12 4");
    EXPECT_EQ(lua("local cells = procedural.voronoi(points, {0, 0, 100, 100}) local total = 0 for _, cell in ipairs(cells) do total = total + m.polygonArea(cell) end return #cells .. ' ' .. math.floor(total + 0.5)"), "5 10000");
    EXPECT_EQ(lua("local relaxed = procedural.relax(points, {0, 0, 100, 100}, 2) return #relaxed .. ' ' .. tostring(relaxed[1].x > 0)"), "5 true");
    await("procedural.voronoiAsync(points, {0, 0, 100, 100})");
    EXPECT_EQ(lua("return #done[1]"), "5");
    await("procedural.relaxAsync(points, {0, 0, 100, 100}, 1)");
    EXPECT_EQ(lua("return #done[1]"), "5");
}

TEST_F(Procedural2DLuaTest, AutotilesByNeighborsAndWangSets) {
    // clang-format off
    fixture.runLua(R"(
        ground = spatial.newCellGrid(3, 3, 0)
        ground:set(1, 0, 1) ground:set(0, 1, 1) ground:set(1, 1, 1) ground:set(2, 1, 1) ground:set(1, 2, 1)
        colors = spatial.newCellGrid(3, 3, 1)
        colors:set(2, 2, 2)
        set = {kind = 'corner', tiles = {{tileId = 4, wangId = {0, 1, 0, 1, 0, 1, 0, 1}}, {tileId = 9, wangId = {0, 1, 0, 2, 0, 1, 0, 1}}}}
    )");
    // clang-format on
    EXPECT_EQ(lua("return procedural.mask4(ground, 1, 1) .. ' ' .. procedural.mask4(ground, 1, 0, false) .. ' ' .. procedural.mask8(ground, 1, 1)"), "15 4 85");
    EXPECT_EQ(lua("return procedural.blobIndex(0) .. ' ' .. procedural.blobIndex(255) .. ' ' .. procedural.blobIndex(2)"), "0 46 -1");
    EXPECT_EQ(lua("local masks = procedural.autotile4(ground, 1) return masks:get(0, 0) .. ' ' .. masks:get(1, 1)"), "-1 15");
    EXPECT_EQ(lua("local blobs = procedural.autotile8(ground, 1) return blobs:get(1, 1) == procedural.blobIndex(85)"), "true");
    EXPECT_EQ(lua("local tiles = procedural.wang(colors, set) return tiles.width .. ' ' .. tiles:get(0, 0) .. ' ' .. tiles:get(1, 1)"), "2 4 9");
    EXPECT_NE(lua("procedural.mask4(ground, 5, 5)").find("outside the grid"), std::string::npos);
    EXPECT_NE(lua("procedural.wang(colors, {kind = 'diagonal', tiles = {}})").find("corner, edge or mixed"), std::string::npos);
}

} // namespace haylen
