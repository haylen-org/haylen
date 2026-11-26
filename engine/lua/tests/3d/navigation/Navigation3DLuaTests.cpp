#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::navigation3d {

// Bakes and walks navigation meshes from Lua on the headless engine: paths and filters by area names, obstacles and links that rebuild tiles, bytes, assets and crowds read and steered in float buffers.
class Navigation3DLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // clang-format off
        fixture.runLua(R"(
            m = require('haylen.math')
            navigation3d = require('haylen.navigation3d')
            collections = require('haylen.collections')
            function floor(geometry, side, area)
                local h = side / 2
                geometry:addMesh({m.vec3(-h, 0, -h), m.vec3(-h, 0, h), m.vec3(h, 0, h), m.vec3(h, 0, -h)}, {1, 2, 3, 1, 3, 4}, {area = area})
            end
            function wall(geometry, center, half)
                geometry:addBox(m.obb(center, nil, half))
            end
            room = navigation3d.newGeometry()
            floor(room, 30)
            wall(room, m.vec3(0, 1.5, -8), m.vec3(0.25, 1.5, 7))
            wall(room, m.vec3(0, 1.5, 8), m.vec3(0.25, 1.5, 7))
            settings = {tileSize = 12, agentRadius = 0.4}
        )");
        // clang-format on
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    // Runs an asynchronous call in a task and waits until it stores its result in the global `done`.
    void await(const std::string& call) {
        fixture.runLua("done = nil require('async').spawn(function() done = {" + call + ":await()} end)");
        ASSERT_TRUE(fixture.frameUntil([this] { return lua("return done ~= nil") == "true"; }, std::chrono::seconds(10)));
    }

    test::LuaFixture fixture;
};

TEST_F(Navigation3DLuaTest, BakesAGeometryAndFindsThePathThroughTheDoor) {
    fixture.runLua("mesh = navigation3d.build(room, settings)");

    EXPECT_EQ(lua("local path = mesh:findPath({-10, 0, 8}, {10, 0, 8}) return #path.points >= 3 and tostring(path.partial) .. ' ' .. #path.links"), "false " + lua("local path = mesh:findPath({-10, 0, 8}, {10, 0, 8}) return #path.points"));
    EXPECT_EQ(lua("return tostring(mesh:findPath({-10, 0, 8}, {10, 0, 80}))"), "nil");
    EXPECT_EQ(lua("local near = mesh:findNearest({5, 0.5, 5}) return near.area .. ' ' .. tostring(near.polygon > 0)"), "ground true");
    EXPECT_EQ(lua("local hit = mesh:raycast({-10, 0, 8}, {10, 0, 8}) return tostring(hit.hit) .. ' ' .. tostring(hit.normal.x < -0.99)"), "true true");
    EXPECT_EQ(lua("return mesh.areas[1] .. ' ' .. mesh.settings.tileSize .. ' ' .. mesh.settings.partition .. ' ' .. tostring(mesh.tileCount > 0) .. ' ' .. tostring(mesh.upToDate)"), "ground 12.0 watershed true true");
    EXPECT_EQ(lua("local wall = mesh:findDistanceToWall({-2, 0, 8}, 5) return tostring(wall.distance < 2)"), "true");
    EXPECT_EQ(lua("return #mesh:triangles() % 9"), "0");
}

TEST_F(Navigation3DLuaTest, AnswersWithClearSentencesForWrongOptionsAndAreas) {
    fixture.runLua("mesh = navigation3d.build(room, settings)");

    EXPECT_NE(lua("navigation3d.build(room, {tileSize = 9.6, agentRadio = 1})").find("Unknown option \"agentRadio\"."), std::string::npos);
    EXPECT_NE(lua("navigation3d.build(room, {agentMaxSlope = 3})").find("The setting \"agentMaxSlope\" of a navigation mesh must be an angle in radians"), std::string::npos);
    EXPECT_NE(lua("mesh:newFilter({costs = {lava = 3}})").find("The navigation mesh has no area \"lava\". Name it in the geometry that the mesh bakes from."), std::string::npos);
    EXPECT_NE(lua("mesh:addObstacle({shape = 'box', position = {0, 0, 0}})").find("This navigation mesh was baked without obstacles."), std::string::npos);
    EXPECT_NE(lua("navigation3d.build(navigation3d.newGeometry())").find("The geometry of a navigation mesh holds no triangles, so there is nothing to bake."), std::string::npos);
    EXPECT_NE(lua("navigation3d.decode('HNAV', 'levels/cave.navmesh')").find("The navigation mesh \"levels/cave.navmesh\" is damaged or was not made by this version of Haylen."), std::string::npos);
}

TEST_F(Navigation3DLuaTest, WeighsAndSkipsAreasByTheirNames) {
    // clang-format off
    fixture.runLua(R"(
        field = navigation3d.newGeometry()
        floor(field, 30, 'grass')
        field:addVolume({m.vec3(-15, 0, 6), m.vec3(-15, 0, 9), m.vec3(15, 0, 9), m.vec3(15, 0, 6)}, -1, 1, {area = 'road'})
        mesh = navigation3d.build(field, settings)
        slow = mesh:newFilter({costs = {grass = 10}})
        noRoad = mesh:newFilter({excluded = {'road'}})
    )");
    // clang-format on

    EXPECT_EQ(lua("return table.concat(field.areas, ' ') .. ' ' .. mesh:area('road') .. ' ' .. slow:cost('grass') .. ' ' .. tostring(noRoad:excluded('road'))"), "ground grass road 3 10.0 true");
    EXPECT_EQ(lua("return tostring(mesh:findPath({-12, 0, 0}, {12, 0, 0}, {filter = slow}).length > 28)"), "true");
    EXPECT_EQ(lua("return tostring(mesh:findNearest({0, 0, 7.5}, {filter = noRoad, searchExtents = {0.5, 1, 0.5}}))"), "nil");
}

TEST_F(Navigation3DLuaTest, PicksRandomPointsFromASeedOrAGenerator) {
    fixture.runLua("mesh = navigation3d.build(room, settings)");

    EXPECT_EQ(lua("return tostring(mesh:findRandomPoint({seed = 5}).position) == tostring(mesh:findRandomPoint({seed = 5}).position)"), "true");
    EXPECT_EQ(lua("local random = m.random(3) local a = mesh:findRandomPoint({random = random}) local b = mesh:findRandomPoint({random = random}) return tostring(tostring(a.position) ~= tostring(b.position))"), "true");
    EXPECT_EQ(lua("local near = mesh:findRandomPointAround({-6, 0, 0}, 2, {seed = 1}) return tostring((near.position - m.vec3(-6, near.position.y, 0)):length() <= 2.001)"), "true");
}

TEST_F(Navigation3DLuaTest, CarvesObstaclesAndAddsLinksThatRebuildTiles) {
    // clang-format off
    fixture.runLua(R"(
        carved = navigation3d.build(room, {tileSize = 12, agentRadius = 0.4, obstacles = true})
        crate = carved:addObstacle({shape = 'box', position = {0, 0.5, 0}, halfExtents = {0.8, 0.5, 1.2}})
    )");
    // clang-format on
    await("carved:updatedAsync()");

    EXPECT_EQ(lua("return tostring(carved:findPath({-10, 0, 0}, {10, 0, 0}).partial) .. ' ' .. carved:obstacle(crate).shape .. ' ' .. carved.obstacleCount"), "true box 1");
    EXPECT_EQ(lua("return tostring(carved:removeObstacle(crate)) .. ' ' .. tostring(carved:removeObstacle(crate))"), "true false");
    await("carved:updatedAsync()");
    EXPECT_EQ(lua("return tostring(carved:findPath({-10, 0, 0}, {10, 0, 0}).partial)"), "false");

    fixture.runLua("ladder = carved:addLink({-1, 0, -10}, {1, 0, -10}, {kind = 'ladder', bidirectional = false})");
    await("carved:updatedAsync()");
    EXPECT_EQ(lua("local link = carved:link(ladder) return link.kind .. ' ' .. tostring(link.bidirectional) .. ' ' .. carved.linkCount"), "ladder false 1");
}

TEST_F(Navigation3DLuaTest, RebuildsTheTilesOfAChangedGeometryInTheBackground) {
    // clang-format off
    fixture.runLua(R"(
        mesh = navigation3d.build(room, settings)
        open = navigation3d.newGeometry()
        floor(open, 30)
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(mesh:findPath({-10, 0, 8}, {10, 0, 8}).length > 20.5)"), "true");

    await("mesh:rebuildAsync(open, m.box({-1, -1, -15}, {1, 3, 15}))");
    EXPECT_EQ(lua("return tostring(done[1]) .. ' ' .. string.format('%.1f', mesh:findPath({-10, 0, 8}, {10, 0, 8}).length)"), "true 20.0");
}

TEST_F(Navigation3DLuaTest, WritesBytesThatDecodeIntoTheSameMesh) {
    fixture.runLua("mesh = navigation3d.build(room, settings) bytes = mesh:encode() copy = navigation3d.decode(bytes, 'room.navmesh')");

    EXPECT_EQ(lua("return tostring(copy:encode() == bytes) .. ' ' .. tostring(copy.polygonCount == mesh.polygonCount)"), "true true");
}

TEST_F(Navigation3DLuaTest, BakesInTheBackground) {
    await("navigation3d.buildAsync(room, settings)");

    EXPECT_EQ(lua("return tostring(done[1].tileCount > 0) .. ' ' .. tostring(done[1]:findPath({-10, 0, 8}, {10, 0, 8}).partial)"), "true false");
}

TEST_F(Navigation3DLuaTest, StepsACrowdAndReadsEveryAgentInOneCall) {
    // clang-format off
    fixture.runLua(R"(
        mesh = navigation3d.build(room, settings)
        crowd = navigation3d.newCrowd(mesh, {maxAgents = 8})
        for index = 1, 6 do
            crowd:addAgent({position = {-10, 0, -12 + index * 3}, radius = 0.4, maxSpeed = 4})
        end
        targets = collections.newFloatBuffer(6 * 3)
        for index = 1, 6 do
            targets:set(index * 3 - 2, {10, 0, -12 + index * 3})
        end
        crowd:setTargets(targets)
        positions = collections.newFloatBuffer(6 * 3)
        for frame = 1, 200 do crowd:step(0.1) end
        crowd:readPositions(positions)
    )");
    // clang-format on

    EXPECT_EQ(lua("return crowd.count .. ' ' .. #crowd.agents"), "6 6");
    EXPECT_EQ(lua("local arrived = 0 for index = 1, 6 do if math.abs(positions[index * 3 - 2] - 10) < 1 then arrived = arrived + 1 end end return arrived"), "6");
    EXPECT_EQ(lua("local id = crowd.agents[1] return crowd:state(id) .. ' ' .. tostring(crowd:partial(id)) .. ' ' .. crowd:target(id).x .. ' ' .. crowd:target(id).z"), "walking false 10.0 -9.0");
    EXPECT_NE(lua("crowd:addAgent({position = {0, 0, 0}, speed = 3})").find("Unknown option \"speed\"."), std::string::npos);
    EXPECT_NE(lua("crowd:readPositions(collections.newFloatBuffer(3))").find("The positions of 6 agents need 18 numbers, but there is room for 3."), std::string::npos);
}

} // namespace haylen::lua::navigation3d
