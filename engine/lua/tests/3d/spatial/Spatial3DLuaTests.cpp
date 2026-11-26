#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::spatial3d {

// Runs the structures of `haylen.spatial3d` from Lua on the headless engine.
class Spatial3DLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // clang-format off
        fixture.runLua(R"(
            m = require('haylen.math')
            spatial3d = require('haylen.spatial3d')
            collections = require('haylen.collections')
            function names(values)
                local list = {}
                for _, value in ipairs(values) do list[#list + 1] = value.name end
                return table.concat(list, ',')
            end
            function fill(structure, builds)
                goblin = {name = 'goblin'}
                tree = {name = 'tree'}
                rock = {name = 'rock'}
                structure:set(goblin, m.box({0, 0, 0}, {1, 2, 1}))
                structure:set(tree, m.box({4, 0, 0}, {5, 6, 1}))
                structure:set(rock, m.box({40, 0, 40}, {41, 1, 41}))
                if builds then structure:build() end
                return structure
            end
            structures = {
                spatial3d.newHashGrid(2),
                spatial3d.newOctree(m.box({-50, -50, -50}, {50, 50, 50}), {maxEntries = 1, maxDepth = 4, looseness = 1}),
                spatial3d.newAabbTree(0.25),
                spatial3d.newBvh(),
            }
        )");
        // clang-format on
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture;
};

TEST_F(Spatial3DLuaTest, StoresLuaValuesInEveryStructure) {
    for (int structure = 1; structure <= 4; ++structure) {
        const std::string self = "fill(structures[" + std::to_string(structure) + "], " + (structure == 4 ? "true" : "false") + ")";
        EXPECT_EQ(lua("return names(" + self + ":query(m.box({-1, -1, -1}, {4.5, 1, 1})))"), "goblin,tree") << structure;
        EXPECT_EQ(lua("return names(structures[" + std::to_string(structure) + "]:querySphere(m.sphere({40.5, 0.5, 40.5}, 0.1)))"), "rock") << structure;
        EXPECT_EQ(lua("return names(structures[" + std::to_string(structure) + "]:queryPoint({4.5, 5, 0.5}))"), "tree") << structure;
        EXPECT_EQ(lua("return names(structures[" + std::to_string(structure) + "]:kNearest({3, 1, 0.5}, 2))"), "tree,goblin") << structure;
        EXPECT_EQ(lua("return structures[" + std::to_string(structure) + "]:nearest({3, 1, 0.5}, 100, function(value) return value ~= tree end).name"), "goblin") << structure;
        EXPECT_EQ(lua("local hits = structures[" + std::to_string(structure) + "]:raycast(m.ray3d({-10, 0.5, 0.5}, m.right())) return #hits .. ' ' .. hits[1].value.name .. ' ' .. hits[1].distance .. ' ' .. tostring(hits[1].normal)"), "2 goblin 10.0 Vec3(-1.0, 0.0, 0.0)") << structure;
        EXPECT_EQ(lua("local s = structures[" + std::to_string(structure) + "] return tostring(s:has(rock)) .. ' ' .. s.size .. ' ' .. tostring(s:remove(rock)) .. ' ' .. tostring(s:remove(rock)) .. ' ' .. tostring(s:has(rock))"), "true 3 true false false") << structure;
    }

    EXPECT_EQ(lua("return structures[1].cellSize .. ' ' .. structures[3].margin .. ' ' .. tostring(structures[2].region.max) .. ' ' .. tostring(structures[2].nodeCount > 0)"), "2.0 0.25 Vec3(50.0, 50.0, 50.0) true");
}

TEST_F(Spatial3DLuaTest, CullsWithAFrustumIntoAListItReuses) {
    // clang-format off
    lua(R"(
        camera = m.frustum(m.perspective({farPlane = 100}) * m.lookAt({0, 2, 20}, {0, 2, 0}))
        grid = spatial3d.newAabbTree()
        crates = {}
        for index = 1, 30 do
            crates[index] = {name = 'crate' .. index}
            grid:set(crates[index], m.boxFromCenter({(index - 15) * 10, 2, 0}, {1, 1, 1}))
        end
        visible = {}
        for index = 1, 50 do visible[index] = true end
    )");
    // clang-format on
    EXPECT_EQ(lua("local same = grid:queryFrustum(camera, visible) return tostring(same == visible) .. ' ' .. #visible .. ' ' .. tostring(visible[#visible + 1] == nil)"), "true 3 true");
    EXPECT_EQ(lua("grid:queryFrustum(camera, visible) return names(visible)"), "crate14,crate15,crate16");
    EXPECT_EQ(lua("return #grid:query(m.box({-1000, -1, -1}, {1000, 5, 1}), visible) .. ' ' .. #grid:queryPoint({1000, 0, 0}, visible)"), "30 0");
}

TEST_F(Spatial3DLuaTest, SetsAndMovesManyValuesInOneCall) {
    // clang-format off
    lua(R"(
        tree = spatial3d.newBvh()
        values = {}
        bounds = collections.newFloatBuffer(6 * 100)
        for index = 1, 100 do
            values[index] = {name = 'item' .. index}
            local x = index * 3
            bounds:set((index - 1) * 6 + 1, {x, 0, 0, x + 1, 1, 1})
        end
        tree:setAll(values, bounds)
        tree:build()
    )");
    // clang-format on
    EXPECT_EQ(lua("return tree.size .. ' ' .. names(tree:queryPoint({30.5, 0.5, 0.5})) .. ' ' .. tostring(tree.built) .. ' ' .. tostring(tree.depth > 0 and tree.nodeCount > 1)"), "100 item10 true true");

    // Moving every value keeps the shape of the tree, which a refit fits again.
    EXPECT_EQ(lua("for index = 1, 100 do bounds[(index - 1) * 6 + 2] = 10 bounds[(index - 1) * 6 + 5] = 11 end tree:setAll(values, bounds) return tostring(tree.built)"), "false");
    EXPECT_NE(lua("tree:queryPoint({30.5, 10.5, 0.5})").find("The BVH changed since its last build. Call \"build\", or \"refit\" when its entries only moved, before a query."), std::string::npos);
    EXPECT_EQ(lua("tree:refit() return names(tree:queryPoint({30.5, 10.5, 0.5})) .. ' ' .. tostring(tree:bounds(values[10]).min)"), "item10 Vec3(30.0, 10.0, 0.0)");

    // A batch that the structure refuses changes nothing, not even the values it would have added.
    EXPECT_NE(lua("local extra = {name = 'extra'} local buffer = collections.newFloatBuffer(12) buffer:set(1, {0, 0, 0, 1, 1, 1, 5, 0, 0, 4, 1, 1}) tree:setAll({extra, {name = 'wrong'}}, buffer)").find("Spatial bounds must be finite"), std::string::npos);
    EXPECT_EQ(lua("return tree.size .. ' ' .. tostring(tree.built)"), "100 true");
    EXPECT_NE(lua("tree:setAll(values, collections.newFloatBuffer(6))").find("The buffer of a batch of 100 values needs 600 numbers, but it holds 6."), std::string::npos);
    EXPECT_NE(lua("tree:refit() tree:set({}, m.box({0, 0, 0}, {1, 1, 1})) tree:refit()").find("Entries came or went since the last build of the BVH, so it needs \"build\" instead of \"refit\"."), std::string::npos);
}

TEST_F(Spatial3DLuaTest, FindsPointsWithAKdTree) {
    // clang-format off
    lua(R"(
        stars = spatial3d.newKdTree()
        sun = {name = 'sun'}
        moon = {name = 'moon'}
        stars:set(sun, {0, 0, 0}, 5)
        stars:set(moon, {20, 0, 0})
        spheres = collections.newFloatBuffer(8)
        spheres:set(1, {100, 0, 0, 1, 0, 100, 0, 2})
        stars:setAll({{name = 'far'}, {name = 'high'}}, spheres)
    )");
    // clang-format on
    EXPECT_NE(lua("stars:kNearest({0, 0, 0}, 1)").find("needs \"build\" before a query"), std::string::npos);
    EXPECT_EQ(lua("stars:build() return names(stars:kNearest({12, 0, 0}, 2)) .. ' ' .. tostring(stars.built) .. ' ' .. stars:sphere(sun).radius"), "sun,moon true 5.0");
    EXPECT_EQ(lua("return names(stars:querySphere(m.sphere({0, 94, 0}, 4.5)))"), "high");
}

TEST_F(Spatial3DLuaTest, CastsAgainstTheTrianglesOfAMesh) {
    // clang-format off
    lua(R"(
        floor = spatial3d.newMeshBvh({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}, {0, 0, -10}}, {1, 2, 3, 1, 3, 4})
        positions = collections.newFloatBuffer(9)
        positions:set(1, {0, 0, 0, 1, 0, 0, 0, 1, 0})
        wall = spatial3d.newMeshBvh(positions)
    )");
    // clang-format on
    EXPECT_EQ(lua("local hit = floor:raycast(m.ray3d({3, 5, -1}, -m.up())) return hit.triangle .. ' ' .. hit.distance .. ' ' .. tostring(hit.normal) .. ' ' .. tostring(hit.front)"), "1 5.0 Vec3(0.0, 1.0, 0.0) true");
    EXPECT_EQ(lua("local hit = floor:raycast(m.ray3d({1, 5, -8}, -m.up())) return hit.triangle .. ' ' .. tostring(hit.barycentric:approximately({0.2, 0.1, 0.7}))"), "2 true");
    EXPECT_EQ(lua("return tostring(floor:raycast(m.ray3d({3, 5, -1}, m.up()))) .. ' ' .. tostring(floor:intersects(m.ray3d({3, 5, -1}, -m.up())))"), "nil true");
    EXPECT_EQ(lua("local closest = floor:closestPoint({3, 4, -1}) return closest.distance .. ' ' .. tostring(closest.point:approximately({3, 0, -1})) .. ' ' .. tostring(floor:closestPoint({3, 4, -1}, 1))"), "4.0 true nil");
    EXPECT_EQ(lua("local list = floor:queryBox(m.box({8, -1, -1}, {9, 1, 0})) return #list .. ' ' .. list[1] .. ' ' .. #floor:querySphere(m.sphere({5, 0, -5}, 0.1))"), "1 1 2");
    EXPECT_EQ(lua("return floor.triangleCount .. ' ' .. floor.vertexCount .. ' ' .. tostring(floor.bounds.min) .. ' ' .. tostring(floor:triangle(2).c) .. ' ' .. wall.triangleCount"), "2 4 Vec3(0.0, 0.0, -10.0) Vec3(0.0, 0.0, -10.0) 1");
    EXPECT_NE(lua("floor:triangle(3)").find("The triangle count of the mesh is 2, so it has no triangle 3."), std::string::npos);
    EXPECT_NE(lua("spatial3d.newMeshBvh({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}, {1, 2, 4})").find("Item 3 of the indices of a mesh names vertex 4, but the vertex count of the mesh is 3."), std::string::npos);
    EXPECT_NE(lua("spatial3d.newMeshBvh({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}, {1, 2, 0})").find("Item 3 of the indices of a mesh must name a vertex as a whole number from 1 on."), std::string::npos);
}

TEST_F(Spatial3DLuaTest, WalksTheCellsOfAGrid) {
    EXPECT_EQ(lua("local cells = spatial3d.traverseGrid({0.5, 0.5, 0.5}, {2.5, 0.5, 0.5}, 1) return #cells .. ' ' .. cells[3].x .. ' ' .. cells[3].distance .. ' ' .. tostring(cells[3].normal)"), "3 2 1.5 Vec3(-1.0, 0.0, 0.0)");
    EXPECT_EQ(lua("local cells = spatial3d.traverseGrid({0, 0, 0}, {0, -3, 0}, m.vec3(1, 2, 1)) return #cells .. ' ' .. cells[2].y"), "3 -1");
    EXPECT_NE(lua("spatial3d.traverseGrid({0, 0, 0}, {100000, 0, 0}, 1)").find("A grid traversal lists at most 65536 cells."), std::string::npos);
}

TEST_F(Spatial3DLuaTest, RefusesWrongSettings) {
    EXPECT_NE(lua("spatial3d.newOctree(m.box({0, 0, 0}, {1, 1, 1}), {depth = 2})").find("Unknown option \"depth\"."), std::string::npos);
    EXPECT_NE(lua("spatial3d.newHashGrid(0)").find("A spatial hash needs a positive cell size."), std::string::npos);
    EXPECT_NE(lua("spatial3d.newAabbTree(-1)").find("An AABB tree needs a finite margin of zero or more."), std::string::npos);
    EXPECT_NE(lua("structures[1]:set(nil, m.box({0, 0, 0}, {1, 1, 1}))").find("a value to store is required"), std::string::npos);
    EXPECT_EQ(lua("local ok = pcall(structures[1].set, structures[1], {}, m.box({1, 1, 1}, {0, 0, 0})) return tostring(ok) .. ' ' .. structures[1].size"), "false 0");
}

} // namespace haylen::lua::spatial3d
