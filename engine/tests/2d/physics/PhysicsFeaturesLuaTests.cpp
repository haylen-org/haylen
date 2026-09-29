#include <gtest/gtest.h>

#include <string>

#include "support/EngineFixture.hpp"

namespace haylen {

class PhysicsFeaturesLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("physics2d = require('haylen.physics2d') world = physics2d.newWorld() function run(seconds) for i = 1, math.floor(seconds * 60) do world:step(1 / 60) end end");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::EngineFixture fixture;
};

TEST_F(PhysicsFeaturesLuaTest, BuildsRopesBridgesRagdollsAndVehicles) {
    // clang-format off
    fixture.runLua(R"(
        hook = world:createBody({type = 'static', x = 0, y = 0})
        rope = physics2d.newRope(world, {from = {0, 0}, to = {100, 0}, segments = 5, startBody = hook, linearDamping = 1})
        bridge = physics2d.newBridge(world, {from = {200, 100}, to = {400, 100}, segments = 8, thickness = 6})
        doll = physics2d.newRagdoll(world, {x = 600, y = 0, height = 100, jointFriction = 5})
        ground = world:createBody({type = 'static', x = 1000, y = 300})
        ground:addBox(2000, 20)
        car = physics2d.newVehicle(world, {x = 900, y = 250, chassisWidth = 100, chassisHeight = 20, rearWheel = {-35, 15}, frontWheel = {35, 15}, drive = 'all'})
    )");
    // clang-format on
    EXPECT_EQ(lua("return #rope:bodies() .. ' ' .. #rope:joints() .. ' ' .. rope.segmentLength .. ' ' .. #rope:points() .. ' ' .. tostring(rope.valid)"), "5 5 20.0 6 true");
    EXPECT_EQ(lua("local segments = rope:segments() return #segments .. ' ' .. segments[1].x .. ' ' .. segments[1].length"), "5 10.0 20.0");
    EXPECT_EQ(lua("return #bridge:bodies() .. ' ' .. #bridge:joints()"), "8 9");
    EXPECT_EQ(lua("local bodies = doll:bodies() return tostring(bodies.head ~= nil) .. ' ' .. tostring(doll:body('chest') == bodies.chest) .. ' ' .. #doll:joints()"), "true true 10");
    EXPECT_EQ(lua("return car.drive .. ' ' .. #car:joints() .. ' ' .. tostring(car.chassis.valid) .. ' ' .. tostring(car.rearWheel.x < car.frontWheel.x)"), "all 2 true true");

    fixture.runLua("run(1) start = car.chassis.x car.motorSpeed = 12 run(2)");
    EXPECT_EQ(lua("return tostring(car.chassis.x > start + 50) .. ' ' .. car.motorSpeed"), "true 12.0");
    EXPECT_EQ(lua("return tostring(rope:points()[6].y > 30)"), "true");

    fixture.runLua("rope:destroy() doll:destroy() car:destroy() bridge:destroy()");
    EXPECT_EQ(lua("return tostring(rope.valid) .. ' ' .. tostring(doll.valid) .. ' ' .. tostring(car.valid) .. ' ' .. world.bodyCount"), "false false false 2");
    EXPECT_NE(lua("doll:body('tail')").find("unknown ragdoll part"), std::string::npos);
    EXPECT_NE(lua("physics2d.newRope(world, {from = {0, 0}, to = {0, 0}})").find("two distinct ends"), std::string::npos);
    EXPECT_NE(lua("physics2d.newVehicle(world, {drive = 'tracks'})").find("unknown value 'tracks'"), std::string::npos);
    EXPECT_NE(lua("physics2d.newRagdoll(world, {size = 2})").find("Unknown option 'size'"), std::string::npos);
}

TEST_F(PhysicsFeaturesLuaTest, OneWayPlatformsAndConveyors) {
    // clang-format off
    fixture.runLua(R"(
        platform = world:createBody({type = 'static', x = 0, y = 0})
        surface = platform:addBox(200, 10, {oneWay = {0, -1}})
        ball = world:createBody({x = 0, y = 60, vy = -700})
        ball:addCircle(8)
        belt = world:createBody({type = 'static', x = 5000, y = 0})
        conveyor = belt:addBox(2000, 10, {friction = 0.8, tangentSpeed = 100})
        crate = world:createBody({x = 5000, y = -20})
        crate:addBox(20, 20)
        run(2.5)
    )");
    // clang-format on
    EXPECT_EQ(lua("return surface.oneWay.y .. ' ' .. tostring(ball.y < -5) .. ' ' .. conveyor.tangentSpeed"), "-1.0 true 100.0");
    EXPECT_EQ(lua("return tostring(crate.x > 5100)"), "true");
    EXPECT_EQ(lua("surface.oneWay = nil conveyor.tangentSpeed = 0 return tostring(surface.oneWay) .. ' ' .. conveyor.tangentSpeed"), "nil 0.0");
}

TEST_F(PhysicsFeaturesLuaTest, CarvesTerrainAndBreaksBodies) {
    // clang-format off
    fixture.runLua(R"(
        terrain = physics2d.newTerrain(world, {columns = 65, rows = 33, cellSize = 8, chunkSize = 16})
        terrain:setSamples(function(column, row) return row >= 16 and 1 or 0 end)
        built = terrain:update()
        crate = world:createBody({x = 200, y = 100})
        crate:addBox(20, 20)
        run(1)
    )");
    // clang-format on
    EXPECT_EQ(lua("return terrain.columns .. ' ' .. terrain.rows .. ' ' .. terrain.cellSize .. ' ' .. terrain.chunkCount .. ' ' .. built .. ' ' .. #terrain:bodies()"), "65 33 8.0 8 8 8");
    EXPECT_EQ(lua("return tostring(terrain:isSolid({100, 200})) .. ' ' .. terrain:sample(0, 20) .. ' ' .. #terrain:samples() .. ' ' .. terrain.bounds.width"), "true 1.0 2145 512.0");
    EXPECT_EQ(lua("return tostring(#terrain:outlines() >= 4)"), "true");

    // clang-format off
    fixture.runLua(R"(
        hits = terrain:explode(180, 130, 24, {impulse = 600})
        dirty = terrain.dirtyChunkCount
        rebuilt = terrain:update()
        terrain:carve(400, 128, 10)
        terrain:fill(400, 128, 20)
        terrain:carvePolygon({{0, 120}, {30, 120}, {30, 140}, {0, 140}})
        terrain:fillPolygon({{0, 120}, {30, 120}, {30, 140}, {0, 140}})
    )");
    // clang-format on
    EXPECT_EQ(lua("return #hits .. ' ' .. tostring(hits[1].body == crate) .. ' ' .. tostring(hits[1].impulseX > 0) .. ' ' .. tostring(dirty > 0 and rebuilt == dirty)"), "1 true true true");
    EXPECT_EQ(lua("return tostring(terrain:isSolid({180, 140})) .. ' ' .. tostring(terrain:isSolid({400, 140}))"), "false true");

    fixture.runLua("world.gravity = {0, 0} blast = physics2d.explode(world, {x = 0, y = 0, radius = 1000, impulse = 50, falloff = 'none', occlusion = false})");
    EXPECT_EQ(lua("return #blast .. ' ' .. math.floor(math.sqrt(blast[1].impulseX ^ 2 + blast[1].impulseY ^ 2) + 0.5)"), "1 50");

    // clang-format off
    fixture.runLua(R"(
        glass = world:createBody({x = 300, y = -200})
        glass:addBox(60, 40)
        pieces = physics2d.fracture(glass, {pieces = 6, impact = {310, -200}, seed = 3})
        parts = physics2d.splitPolygon({{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {pieces = 5, seed = 2, minimumArea = 0})
        area = 0
        for _, part in ipairs(parts) do area = area + require('haylen.math').polygon.area(part) end
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(#pieces >= 4) .. ' ' .. tostring(glass.valid) .. ' ' .. tostring(pieces[1].valid) .. ' ' .. #parts .. ' ' .. math.floor(area + 0.5)"), "true false true 5 10000");
    EXPECT_NE(lua("physics2d.newTerrain(world, {columns = 1})").find("at least 2 by 2"), std::string::npos);
    EXPECT_NE(lua("terrain:setSamples({1, 2})").find("one value per sample"), std::string::npos);
    EXPECT_NE(lua("physics2d.explode(world, {x = 0, y = 0, radius = 10, falloff = 'cubic'})").find("unknown value 'cubic'"), std::string::npos);
}

TEST_F(PhysicsFeaturesLuaTest, ReadsTheGeometryOfShapesForDrawing) {
    // clang-format off
    fixture.runLua(R"(
        body = world:createBody({type = 'static', x = 100, y = 50})
        box = body:addBox(40, 20)
        ball = body:addCircle(12, {offsetY = 30})
        pill = body:addCapsule(0, -10, 0, 10, 6)
        line = body:addSegment(-50, 0, 50, 0)
        ground = world:createBody({type = 'static'})
        ground:addChain({{0, 0}, {100, 0}, {100, 100}, {0, 100}}, true)
        glass = world:createBody({x = 300, y = -200})
        glass:addBox(64, 64)
        pieces = physics2d.fracture(glass)
    )");
    // clang-format on
    EXPECT_EQ(lua("local corner, placed = box.points[1], box.worldPoints[1] return box.kind .. ' ' .. #box.points .. ' ' .. box.radius .. ' ' .. math.abs(corner.x) .. ',' .. math.abs(corner.y) .. ' ' .. placed.x - corner.x .. ',' .. placed.y - corner.y"), "polygon 4 0.0 20.0,10.0 100.0,50.0");
    EXPECT_EQ(lua("return ball.kind .. ' ' .. ball.points[1].y .. ' ' .. ball.worldPoints[1].x .. ',' .. ball.worldPoints[1].y .. ' ' .. ball.radius"), "circle 30.0 100.0,80.0 12.0");
    EXPECT_EQ(lua("return pill.kind .. ' ' .. #pill.points .. ' ' .. pill.radius .. ' ' .. line.kind .. ' ' .. line.worldPoints[2].x .. ' ' .. tostring(line:outline().closed)"), "capsule 2 6.0 segment 150.0 false");
    EXPECT_EQ(lua("local outline = ball:outline() return #outline.points .. ' ' .. tostring(outline.closed) .. ' ' .. #body:outlines()"), "16 true 4");
    EXPECT_EQ(lua("local outlines = ground:outlines() return #outlines .. ' ' .. #outlines[1].points .. ' ' .. tostring(outlines[1].closed) .. ' ' .. ground:shapes()[1].kind"), "1 4 true chainSegment");

    // The pieces of a fracture are polygons whose outlines draw them where they are.
    EXPECT_EQ(lua("local shape = pieces[1]:shapes()[1] local outline = pieces[1]:outlines()[1] return shape.kind .. ' ' .. tostring(#shape.points >= 3) .. ' ' .. tostring(outline.closed) .. ' ' .. tostring(#outline.points == #shape.worldPoints)"), "polygon true true true");
    EXPECT_NE(lua("local shape = box box:destroy() return shape.points").find("The shape was destroyed."), std::string::npos);
}

TEST_F(PhysicsFeaturesLuaTest, FluidsExposePositionsInBulk) {
    // clang-format off
    fixture.runLua(R"(
        floor = world:createBody({type = 'static', x = 0, y = 200})
        floor:addBox(400, 20)
        water = physics2d.newFluid(world, {radius = 4, smoothingRadius = 16, maxParticles = 100})
        added = water:fill({-40, 0, 80, 40})
        spawned = water:spawn(0, -50, 0, 10)
        for i = 1, 60 do
            water:update(1 / 60)
            world:step(1 / 60)
        end
        buffer = {}
        water:positions(buffer)
        velocities = water:velocities()
    )");
    // clang-format on
    EXPECT_EQ(lua("return added .. ' ' .. tostring(spawned) .. ' ' .. water.count .. ' ' .. water.radius .. ' ' .. #buffer .. ' ' .. #velocities .. ' ' .. #water:bodies()"), "50 true 51 4.0 102 102 51");
    EXPECT_EQ(lua("water:remove(1) water:positions(buffer) return water.count .. ' ' .. #buffer .. ' ' .. tostring(buffer[101])"), "50 100 nil");
    EXPECT_EQ(lua("water:clear() return water.count .. ' ' .. world.bodyCount"), "0 1");
    EXPECT_NE(lua("water:remove(0)").find("particles count from 1"), std::string::npos);
    EXPECT_NE(lua("physics2d.newFluid(world, {radius = 4, smoothingRadius = 3})").find("larger smoothing radius"), std::string::npos);
}

} // namespace haylen
