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
        doll = physics2d.newRagdoll(world, {x = 600, y = 0, height = 100, stiffness = 0.3, category = 2, mask = 1})
        ground = world:createBody({type = 'static', x = 1000, y = 300})
        ground:addBox(2000, 20)
        car = physics2d.newVehicle(world, {x = 900, y = 250, chassisWidth = 100, chassisHeight = 20, rearWheel = {-35, 15}, frontWheel = {35, 15}, drive = 'all'})
    )");
    // clang-format on
    EXPECT_EQ(lua("return #rope:bodies() .. ' ' .. #rope:joints() .. ' ' .. rope.segmentLength .. ' ' .. #rope:points() .. ' ' .. tostring(rope.valid)"), "5 6 20.0 6 true");
    EXPECT_EQ(lua("local segments = rope:segments() return #segments .. ' ' .. segments[1].x .. ' ' .. segments[1].length"), "5 10.0 20.0");
    EXPECT_EQ(lua("return #bridge:bodies() .. ' ' .. #bridge:joints()"), "8 10");
    EXPECT_EQ(lua("local bodies = doll:bodies() return tostring(bodies.head ~= nil) .. ' ' .. tostring(doll:body('chest') == bodies.chest) .. ' ' .. #doll:joints() .. ' ' .. doll:body('head'):shapes()[1].category .. ' ' .. tostring(doll.mass > 0)"), "true true 10 2 true");
    EXPECT_EQ(lua("return car.drive .. ' ' .. #car:joints() .. ' ' .. tostring(car.chassis.valid) .. ' ' .. tostring(car.rearWheel.x < car.frontWheel.x)"), "all 2 true true");

    fixture.runLua("run(1) start = car.chassis.x car.throttle = 1 run(1)");
    EXPECT_EQ(lua("return tostring(car.chassis.x > start + 50) .. ' ' .. car.throttle .. ' ' .. tostring(car.grounded) .. ' ' .. tostring(car.speed > 100)"), "true 1.0 true true");
    fixture.runLua("car.throttle = 0 car.brake = 1 run(2)");
    EXPECT_EQ(lua("return car.brake .. ' ' .. math.floor(math.abs(car.speed))"), "1.0 0");
    EXPECT_EQ(lua("return tostring(rope:points()[6].y > 30)"), "true");

    fixture.runLua("rope:destroy() doll:destroy() car:destroy() bridge:destroy()");
    EXPECT_EQ(lua("return tostring(rope.valid) .. ' ' .. tostring(doll.valid) .. ' ' .. tostring(car.valid) .. ' ' .. world.bodyCount"), "false false false 2");
    EXPECT_NE(lua("doll:body('tail')").find("unknown ragdoll part"), std::string::npos);
    EXPECT_NE(lua("physics2d.newRope(world, {from = {0, 0}, to = {0, 0}})").find("two distinct ends"), std::string::npos);
    EXPECT_NE(lua("physics2d.newVehicle(world, {drive = 'tracks'})").find("unknown value 'tracks'"), std::string::npos);
    EXPECT_NE(lua("physics2d.newRagdoll(world, {size = 2})").find("Unknown option \"size\""), std::string::npos);
    EXPECT_NE(lua("physics2d.newRagdoll(world, {stiffness = 2})").find("A ragdoll needs a positive height, a negative collision group and a stiffness from 0 to 1."), std::string::npos);
    EXPECT_NE(lua("physics2d.newVehicle(world, {topSpeed = 0})").find("A vehicle needs"), std::string::npos);
    EXPECT_NE(lua("return car.motorSpeed").find("has no member \"motorSpeed\""), std::string::npos);
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
        terrain.samples = function(column, row) return row >= 16 and 1 or 0 end
        built = terrain:update()
        crate = world:createBody({x = 200, y = 100})
        crate:addBox(20, 20)
        run(1)
    )");
    // clang-format on
    EXPECT_EQ(lua("return terrain.columns .. ' ' .. terrain.rows .. ' ' .. terrain.cellSize .. ' ' .. terrain.chunkCount .. ' ' .. built .. ' ' .. #terrain:bodies()"), "65 33 8.0 8 8 8");
    EXPECT_EQ(lua("return tostring(terrain:isSolid({100, 200})) .. ' ' .. terrain:sample(0, 20) .. ' ' .. #terrain.samples .. ' ' .. terrain.bounds.width"), "true 1.0 2145 512.0");
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
    EXPECT_NE(lua("terrain.samples = {1, 2}").find("The terrain needs one value per sample."), std::string::npos);
    EXPECT_NE(lua("physics2d.newTerrain(world, {friction = -1})").find("A physics shape needs a finite density, friction, restitution and rolling resistance of zero or more."), std::string::npos);
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
        water = physics2d.newFluid(world, {radius = 4, smoothingRadius = 16, maxParticles = 100, viscosity = 0.2, gravityScale = 1})
        added = water:fill({-40, 0, 80, 40})
        spawned = water:spawn(0, -50, 0, 10)
        run(1)
        buffer = {}
        water:positions(buffer)
        velocities = water:velocities()
        floats = require('haylen.collections').newFloatBuffer(water.size * 2 + 2)
        water:positions(floats, 3)
    )");
    // clang-format on
    EXPECT_EQ(lua("return added .. ' ' .. tostring(spawned) .. ' ' .. water.size .. ' ' .. water.radius .. ' ' .. #buffer .. ' ' .. #velocities .. ' ' .. tostring(water.stepMilliseconds >= 0)"), "50 true 51 4.0 102 102 true");
    EXPECT_EQ(lua("return tostring(floats:get(3) == buffer[1]) .. ' ' .. tostring(floats:get(4) == buffer[2]) .. ' ' .. tostring(buffer[2] < 200)"), "true true true");
    EXPECT_EQ(lua("water:remove(1) water:positions(buffer) return water.size .. ' ' .. #buffer .. ' ' .. tostring(buffer[101])"), "50 100 nil");
    EXPECT_NE(lua("water:positions(require('haylen.collections').newFloatBuffer(1))").find("the buffer needs two values for each particle"), std::string::npos);

    // The particles draw as metaballs straight from the fluid.
    fixture.runLua("require('haylen.scene').push({render = function() require('haylen.graphics2d').beginWorld(require('haylen.graphics2d').newCamera()) water:draw({color = '#FF4FA3F7', threshold = 0.5, layer = 2}) end})");
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(lua("water:clear() return water.size .. ' ' .. world.bodyCount"), "0 1");
    EXPECT_NE(lua("water:remove(0)").find("particles count from 1"), std::string::npos);
    EXPECT_NE(lua("water:draw({size = 2})").find("Unknown option \"size\""), std::string::npos);
    EXPECT_NE(lua("physics2d.newFluid(world, {radius = 4, smoothingRadius = 3})").find("larger smoothing radius"), std::string::npos);
    EXPECT_NE(lua("physics2d.newFluid(world, {density = -1})").find("A fluid needs"), std::string::npos);
}

TEST_F(PhysicsFeaturesLuaTest, ReleasesTheDataOfBodiesHoweverTheyAreDestroyed) {
    // clang-format off
    fixture.runLua(R"(
        released = setmetatable({}, {__mode = 'v'})
        local glass = world:createBody({x = 0, y = -500})
        glass:addBox(40, 40)
        glass.data = {}
        released.glass = glass.data
        local rope = physics2d.newRope(world, {from = {0, -800}, to = {100, -800}, segments = 2})
        rope:bodies()[1].data = {}
        released.rope = rope:bodies()[1].data
        local doll = physics2d.newRagdoll(world, {x = 0, y = -1200})
        doll:body('head').data = {}
        released.doll = doll:body('head').data
        kept = world:createBody({x = 300, y = -500})
        kept.data = {}
        released.kept = kept.data
        physics2d.fracture(glass)
        rope:destroy()
        doll:destroy()
        world:step(0)
        collectgarbage()
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(released.glass) .. ' ' .. tostring(released.rope) .. ' ' .. tostring(released.doll) .. ' ' .. tostring(released.kept == kept.data)"), "nil nil nil true");
}

TEST_F(PhysicsFeaturesLuaTest, TunesWorldsAndReadsTheirState) {
    // clang-format off
    fixture.runLua(R"(
        tuned = physics2d.newWorld({gravity = {0, 980}, subSteps = 6, threads = 2, continuous = false, sleepEnabled = false, interpolate = true, contactHertz = 40, contactDampingRatio = 5, contactPushSpeed = 300, maxSpeed = 50000, restitutionThreshold = 10, hitThreshold = 20})
        floor = tuned:createBody({type = 'static', x = 0, y = 100})
        floor:addBox(1000, 20)
        crate = tuned:createBody({x = 0, y = 0, fastRotation = true, sleepThreshold = 4})
        crate:addBox(20, 20)
        for i = 1, 30 do tuned:step(1 / 60) end
    )");
    // clang-format on
    EXPECT_EQ(lua("return tuned.subSteps .. ' ' .. tuned.threads .. ' ' .. tostring(tuned.continuous) .. ' ' .. tostring(tuned.sleepEnabled) .. ' ' .. tostring(tuned.interpolate)"), "6 2 false false true");
    EXPECT_EQ(lua("return tuned.contactHertz .. ' ' .. tuned.contactDampingRatio .. ' ' .. math.floor(tuned.contactPushSpeed + 0.5) .. ' ' .. math.floor(tuned.maxSpeed + 0.5) .. ' ' .. math.floor(tuned.restitutionThreshold + 0.5) .. ' ' .. math.floor(tuned.hitThreshold + 0.5)"), "40.0 5.0 300 50000 10 20");
    EXPECT_EQ(lua("tuned.subSteps = 2 tuned.continuous = true tuned.sleepEnabled = true tuned.maxSpeed = 20000 tuned.contactHertz = 30 tuned.contactDampingRatio = 10 tuned.contactPushSpeed = 192 tuned.restitutionThreshold = 64 tuned.hitThreshold = 64 return tuned.subSteps .. ' ' .. tostring(tuned.continuous) .. ' ' .. math.floor(tuned.maxSpeed + 0.5)"), "2 true 20000");
    EXPECT_EQ(lua("local stats = tuned:stats() return stats.bodies .. ' ' .. stats.awakeBodies .. ' ' .. tostring(stats.stepMilliseconds >= 0) .. ' ' .. tostring(stats.contacts >= 1) .. ' ' .. tuned.awakeBodyCount"), "2 1 true true 1");
    EXPECT_EQ(lua("local hash = tuned:stateHash({crate}) return math.type(hash) .. ' ' .. tostring(hash == tuned:stateHash({crate}))"), "integer true");
    EXPECT_EQ(lua("local x, y, rotation = crate:renderTransform() return tostring(math.abs(y - crate.y) < 20) .. ' ' .. tostring(rotation ~= nil)"), "true true");
    EXPECT_EQ(lua("local buffer = require('haylen.collections').newFloatBuffer(3) tuned:readTransforms({crate}, buffer, 1, true) return tostring(math.abs(buffer:get(2) - crate.y) < 20)"), "true");
    EXPECT_EQ(lua("tuned:wakeAll() return crate.awake"), "true");

    // A predicted throw ends at the floor it would hit.
    EXPECT_EQ(lua("local points, hit = tuned:predictPath(0, -200, 100, 0, {steps = 120, radius = 4}) return tostring(#points > 2) .. ' ' .. tostring(hit.body == floor) .. ' ' .. tostring(points[#points].y < 90)"), "true true true");
    EXPECT_EQ(lua("local points, hit = tuned:predictPath(0, -200, 0, -100, {steps = 5}) return #points .. ' ' .. tostring(hit)"), "6 nil");

    // A joint loaded beyond its break force breaks inside the step that loads it, and the callback receives it.
    // clang-format off
    fixture.runLua(R"(
        broken = nil
        holder = tuned:createBody({type = 'static', x = 400, y = -100})
        shelf = tuned:createBody({x = 400, y = -60})
        shelf:addBox(40, 10)
        weld = tuned:createJoint('weld', holder, shelf, {ax = 400, ay = -100, breakForce = shelf.mass * 980 * 5})
        tuned.onJointBreak = function(joint, info) broken = {valid = joint.valid, a = info.bodyA, force = math.abs(info.forceY)} end
        for i = 1, 10 do tuned:step(1 / 60) end
        intact = weld.valid
        anvil = tuned:createBody({x = 400, y = -150})
        anvil:addBox(40, 40, {density = 20})
        for i = 1, 60 do tuned:step(1 / 60) end
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(intact) .. ' ' .. tostring(weld.valid) .. ' ' .. tostring(broken.valid) .. ' ' .. tostring(broken.a == holder) .. ' ' .. tostring(broken.force > 0)"), "true false false true true");
    EXPECT_NE(lua("tuned.subSteps = 0").find("at least one sub-step"), std::string::npos);
    EXPECT_NE(lua("physics2d.newWorld({threads = 0})").find("at least one thread"), std::string::npos);
    EXPECT_NE(lua("tuned.maxSpeed = -1").find("A physics speed needs to be finite and zero or more."), std::string::npos);
    EXPECT_NE(lua("physics2d.newWorld({warp = 1})").find("Unknown option \"warp\""), std::string::npos);
    EXPECT_NE(lua("physics2d.newWorld():readTransforms({}, require('haylen.collections').newFloatBuffer(3), 1, true)").find("interpolation"), std::string::npos);
}

TEST_F(PhysicsFeaturesLuaTest, ChangesBodiesShapesAndJointsWhileRunning) {
    // clang-format off
    fixture.runLua(R"(
        ground = world:createBody({type = 'static', x = 0, y = 100})
        surface = ground:addBox(2000, 20, {rollingResistance = 0.1, contactEvents = false, hitEvents = false, sensorEvents = true})
        box = world:createBody({x = 0, y = 70})
        skin = box:addBox(40, 40)
        zone = world:createBody({type = 'static', x = 0, y = 50})
        trigger = zone:addBox(200, 40, {sensor = true})
        platform = world:createBody({type = 'kinematic', x = 500, y = 0})
        platform:addBox(100, 10, {oneWay = {0, -1}})
        run(1)
    )");
    // clang-format on
    EXPECT_EQ(lua("return string.format('%.2f', surface.rollingResistance) .. ' ' .. tostring(surface.contactEvents) .. ' ' .. tostring(surface.hitEvents) .. ' ' .. tostring(surface.sensorEvents)"), "0.10 false false true");
    EXPECT_EQ(lua("skin.friction = 0.2 skin.restitution = 0.3 skin.density = 2 skin.rollingResistance = 0.05 skin.hitEvents = false return string.format('%.2f %.2f %.2f %.2f ', skin.friction, skin.restitution, skin.density, skin.rollingResistance) .. tostring(skin.hitEvents)"), "0.20 0.30 2.00 0.05 false");
    EXPECT_EQ(lua("local contacts = box:contacts() return #contacts .. ' ' .. tostring(contacts[1].other == ground) .. ' ' .. tostring(contacts[1].otherShape == surface) .. ' ' .. contacts[1].normalY .. ' ' .. tostring(contacts[1].impulse > 0)"), "1 true true 1.0 true");
    EXPECT_EQ(lua("local inside = trigger:overlaps() return #inside .. ' ' .. tostring(inside[1].body == box)"), "1 true");

    EXPECT_EQ(lua("local mass = box.mass box.mass = mass * 2 return tostring(math.abs(box.mass - mass * 2) < 1e-4) .. ' ' .. tostring(box.inertia > 0)"), "true true");
    EXPECT_EQ(lua("box.centerOfMass = {0, 10} local center = box.centerOfMass return center.y .. ' ' .. math.floor(box.worldCenter.y - box.y + 0.5)"), "10.0 10");
    EXPECT_EQ(lua("box.inertia = 5 local inertia = box.inertia box:resetMass() return string.format('%.1f %.1f', inertia, box.centerOfMass.y)"), "5.0 0.0");
    EXPECT_EQ(lua("box.sleepEnabled = false box.sleepThreshold = 2 return tostring(box.sleepEnabled) .. ' ' .. box.sleepThreshold"), "false 2.0");

    // A platform moved to targets moves by velocity, and the velocity of a point includes the spin.
    EXPECT_EQ(lua("platform:moveTo(510, -6, 0, 1 / 60) return string.format('%.0f %.0f', platform.velocity.x, platform.velocity.y)"), "600 -360");
    EXPECT_EQ(lua("platform.angularVelocity = 2 platform.velocity = {0, 0} local v = platform:velocityAt(500, 50) return string.format('%.0f %.0f', v.x, v.y)"), "-100 0");
    EXPECT_EQ(lua("box:dropThrough() box:dropThrough(0.5) return tostring(box.awake)"), "true");

    // Joints read and change every setting they have, and the members of other joint types raise errors that name the types.
    // clang-format off
    fixture.runLua(R"(
        hinge = world:createJoint('revolute', ground, box, {ax = 0, ay = 70, enableSpring = true, hertz = 3, targetAngle = 0.2})
        rope = world:createJoint('distance', ground, box, {ax = 0, ay = 0, bx = 0, by = 70, enableSpring = true, enableLimit = true, lower = 10, upper = 100})
        slider = world:createJoint('prismatic', ground, box, {ax = 0, ay = 70, axisX = 0, axisY = 1, targetTranslation = 5, breakForce = 1000000})
        puppet = world:createJoint('motor', ground, box, {maxMotorForce = 100, maxMotorTorque = 100})
    )");
    // clang-format on
    EXPECT_EQ(lua("return hinge.type .. ' ' .. tostring(hinge.bodyA == ground) .. ' ' .. tostring(hinge.bodyB == box) .. ' ' .. hinge.hertz .. ' ' .. tostring(math.abs(hinge.targetAngle - 0.2) < 1e-6) .. ' ' .. tostring(hinge.enableSpring)"), "revolute true true 3.0 true true");
    EXPECT_EQ(lua("hinge.enableLimit = true hinge.lower = -0.5 hinge.upper = 0.5 hinge.enableMotor = true hinge.maxMotorTorque = 50 hinge.motorSpeed = 1 hinge.dampingRatio = 0.4 hinge.targetAngle = 0.1 return tostring(hinge.enableLimit) .. ' ' .. hinge.lower .. ' ' .. hinge.upper .. ' ' .. tostring(hinge.enableMotor) .. ' ' .. math.floor(hinge.maxMotorTorque + 0.5) .. ' ' .. hinge.motorSpeed .. ' ' .. tostring(math.abs(hinge.dampingRatio - 0.4) < 1e-6)"), "true -0.5 0.5 true 50 1.0 true");
    EXPECT_EQ(lua("rope.length = 60 rope.upper = 90 return math.floor(rope.length + 0.5) .. ' ' .. math.floor(rope.lower + 0.5) .. ' ' .. math.floor(rope.upper + 0.5) .. ' ' .. tostring(rope.currentLength > 0)"), "60 10 90 true");
    EXPECT_EQ(lua("slider.targetTranslation = 8 return math.floor(slider.targetTranslation + 0.5) .. ' ' .. math.floor(slider.breakForce + 0.5) .. ' ' .. tostring(slider.translation ~= nil) .. ' ' .. tostring(slider.breakTorque)"), "8 1000000 true nil");
    EXPECT_EQ(lua("puppet.linearOffset = {0, -20} puppet.angularOffset = 0.3 puppet.maxMotorForce = 200 return puppet.linearOffset.y .. ' ' .. tostring(math.abs(puppet.angularOffset - 0.3) < 1e-6) .. ' ' .. math.floor(puppet.maxMotorForce + 0.5)"), "-20.0 true 200");
    EXPECT_EQ(lua("hinge.constraintHertz = 30 hinge.constraintDampingRatio = 1 return hinge.constraintHertz .. ' ' .. hinge.constraintDampingRatio .. ' ' .. tostring(hinge.constraintForce ~= nil) .. ' ' .. tostring(hinge.linearSeparation >= 0) .. ' ' .. tostring(hinge.angle ~= nil)"), "30.0 1.0 true true true");
    EXPECT_EQ(lua("slider.breakForce = nil return tostring(slider.breakForce) .. ' ' .. tostring(hinge == hinge)"), "nil true");
    EXPECT_EQ(lua("local a, b = hinge.anchorA, hinge.anchorB return math.floor(a.x + 0.5) .. ' ' .. math.floor(a.y + 0.5) .. ' ' .. tostring(math.abs(b.x - a.x) < 1 and math.abs(b.y - a.y) < 1)"), "0 70 true");
    EXPECT_NE(lua("return puppet.angle").find("Only revolute joints have an angle."), std::string::npos);
    EXPECT_NE(lua("return hinge.length").find("Only distance joints have a length."), std::string::npos);
    EXPECT_NE(lua("puppet.enableLimit = true").find("Only revolute, prismatic, wheel and distance joints have limits."), std::string::npos);
    EXPECT_NE(lua("hinge.lower = 2").find("A joint needs a lower limit that is not above the upper one."), std::string::npos);
    EXPECT_NE(lua("rope.length = 0").find("A distance joint needs a length of at least 0.005 meters."), std::string::npos);
    EXPECT_NE(lua("hinge:destroy() return hinge.hertz").find("The physics joint was destroyed."), std::string::npos);
    EXPECT_NE(lua("box.mass = -1").find("A physics body needs a finite mass"), std::string::npos);
    EXPECT_NE(lua("box:moveTo(0, 0, 0, 0)").find("A physics body moves to a target over a positive time."), std::string::npos);
    EXPECT_NE(lua("return skin:overlaps()").find("Only sensor shapes have overlaps."), std::string::npos);
    EXPECT_NE(lua("skin.friction = -1").find("A physics shape needs a finite density"), std::string::npos);
    EXPECT_NE(lua("world:createBody({spin = 1})").find("Unknown option \"spin\""), std::string::npos);
}

TEST_F(PhysicsFeaturesLuaTest, MovesCharactersAndDragsBodies) {
    // clang-format off
    fixture.runLua(R"(
        ground = world:createBody({type = 'static', x = 0, y = 100})
        ground:addBox(2000, 20)
        step = world:createBody({type = 'static', x = 300, y = 82})
        step:addBox(200, 16)
        hero = physics2d.newMover(world, {x = 0, y = 60, radius = 12, height = 48, stepHeight = 20, snapDistance = 8, maxSlope = 0.8, pushForce = 1000})
        velocity = {x = 0, y = 0}
        for i = 1, 60 do
            velocity = hero:clip(300, velocity.y + 980 / 60)
            hero:move(velocity.x / 60, velocity.y / 60)
            world:step(1 / 60)
        end
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(hero.grounded) .. ' ' .. tostring(hero.x > 250) .. ' ' .. math.floor(hero.y + 0.5) .. ' ' .. hero.groundNormal.y .. ' ' .. tostring(hero.groundBody == step)"), "true true 50 -1.0 true");
    EXPECT_EQ(lua("return hero.radius .. ' ' .. hero.height .. ' ' .. hero.stepHeight .. ' ' .. hero.snapDistance .. ' ' .. tostring(hero.onWall) .. ' ' .. tostring(hero.onCeiling) .. ' ' .. hero.body.type .. ' ' .. hero.groundVelocity.x"), "12.0 48.0 20.0 8.0 false false kinematic 0.0");
    EXPECT_EQ(lua("hero.position = {0, 60} hero.maxSlope = 0.5 hero.snapDistance = 4 return hero.position.x .. ' ' .. hero.maxSlope .. ' ' .. hero.snapDistance"), "0.0 0.5 4.0");
    EXPECT_EQ(lua("local moved = hero:move(10, 0) return math.floor(moved.x + 0.5)"), "10");
    EXPECT_EQ(lua("hero:dropThrough() hero:dropThrough(0.5) return tostring(hero.grounded)"), "false");
    EXPECT_NE(lua("hero:dropThrough(-1)").find("for a finite time of zero or more"), std::string::npos);
    EXPECT_NE(lua("physics2d.newMover(world, {radius = 0})").find("A mover needs"), std::string::npos);
    EXPECT_NE(lua("hero.maxSlope = 2").find("A mover needs a slope limit"), std::string::npos);
    EXPECT_EQ(lua("hero:destroy() return tostring(hero.valid)"), "false");
    EXPECT_NE(lua("hero:move(1, 0)").find("The mover was destroyed."), std::string::npos);

    // clang-format off
    fixture.runLua(R"(
        crate = world:createBody({x = -300, y = 70})
        crate:addBox(40, 40)
        hand = physics2d.newGrabber(world, {pickRadius = 30, strength = 40, hertz = 6, dampingRatio = 0.8})
        taken = hand:grab(-300, 20)
        hand:moveTo(-300, -200)
        run(1)
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(taken == crate) .. ' ' .. tostring(hand.holding) .. ' ' .. tostring(hand.body == crate) .. ' ' .. tostring(crate.y < -100) .. ' ' .. hand.target.y .. ' ' .. tostring(hand.force > 0) .. ' ' .. tostring(math.abs(hand.handle.y + 200) < 40)"), "true true true true -200.0 true true");
    EXPECT_EQ(lua("hand.strength = 10 hand.hertz = 4 hand.dampingRatio = 1 hand.pickRadius = 5 return hand.strength .. ' ' .. hand.hertz .. ' ' .. hand.dampingRatio .. ' ' .. hand.pickRadius"), "10.0 4.0 1.0 5.0");
    EXPECT_EQ(lua("hand:release() return tostring(hand.holding) .. ' ' .. tostring(hand.body) .. ' ' .. tostring(hand:grab(5000, 5000))"), "false nil nil");
    EXPECT_NE(lua("physics2d.newGrabber(world, {strength = 0})").find("A grabber needs a positive strength"), std::string::npos);

    // A pick with a radius finds a thin rod near the pointer, nearest first.
    // clang-format off
    fixture.runLua(R"(
        rod = world:createBody({type = 'static', x = 1000, y = 0})
        rod:addBox(4, 200)
        camera = require('haylen.graphics2d').newCamera()
        camera.anchor = 'topLeft'
    )");
    // clang-format on
    EXPECT_EQ(lua("return #world:pick(camera, 1010, 0) .. ' ' .. tostring(world:pick(camera, 1010, 0, {radius = 12})[1].body == rod)"), "0 true");
}

TEST_F(PhysicsFeaturesLuaTest, PushesBodiesWithFieldsAndDrivesTopDownCars) {
    // clang-format off
    fixture.runLua(R"(
        space = physics2d.newWorld({gravity = {0, 0}})
        ball = space:createBody({x = 200, y = 0})
        ball:addCircle(10)
        magnet = physics2d.newForceField(space, {kind = 'radial', x = 0, y = 0, radius = 400, strength = 1000, falloff = 'linear', linearDrag = 1, mask = 1})
        wind = physics2d.newForceField(space, {kind = 'directional', x = 0, y = 2000, width = 400, height = 400, strength = 500, direction = {0, -1}, acceleration = false, enabled = false})
        water = physics2d.newForceField(space, {kind = 'buoyancy', x = 0, y = 4000, points = {{-200, 0}, {200, 0}, {200, 200}, {-200, 200}}, density = 1.5, linearDrag = 2, angularDrag = 1, flow = {50, 0}})
        for i = 1, 60 do space:step(1 / 60) end
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(ball.x < 200) .. ' ' .. magnet.kind .. ' ' .. magnet.bodyCount .. ' ' .. tostring(magnet.valid) .. ' ' .. magnet.bounds.width"), "true radial 1 true 800.0");
    EXPECT_EQ(lua("magnet.strength = -500 magnet.position = {10, 0} magnet.enabled = false magnet.linearDrag = 0 magnet.angularDrag = 0 return magnet.strength .. ' ' .. magnet.position.x .. ' ' .. tostring(magnet.enabled) .. ' ' .. magnet.linearDrag"), "-500.0 10.0 false 0.0");
    EXPECT_EQ(lua("wind.direction = {1, 0} wind.enabled = true water.density = 2 water.flow = {0, 0} return wind.direction.x .. ' ' .. tostring(wind.enabled) .. ' ' .. water.density .. ' ' .. water.flow.x .. ' ' .. water.kind"), "1.0 true 2.0 0.0 buoyancy");
    EXPECT_EQ(lua("magnet:destroy() return tostring(magnet.valid)"), "false");
    EXPECT_NE(lua("return magnet.strength").find("The force field was destroyed."), std::string::npos);
    EXPECT_NE(lua("physics2d.newForceField(space, {kind = 'tornado', radius = 10})").find("unknown value 'tornado'"), std::string::npos);
    EXPECT_NE(lua("physics2d.newForceField(space, {kind = 'buoyancy', radius = 10})").find("A force field needs"), std::string::npos);

    // clang-format off
    fixture.runLua(R"(
        car = physics2d.newTopDownVehicle(space, {x = 0, y = -2000, rotation = 0, length = 80, width = 40, grip = 1200, steeringLock = 0.5, drive = 'all'})
        car.throttle = 1
        for i = 1, 60 do space:step(1 / 60) end
        cruising = car.speed
        car.steering = 1
        car.handbrake = true
        drifted = false
        for i = 1, 60 do space:step(1 / 60) drifted = drifted or car.drifting end
    )");
    // clang-format on
    EXPECT_EQ(lua("return tostring(cruising > 300) .. ' ' .. tostring(drifted) .. ' ' .. car.throttle .. ' ' .. car.steering .. ' ' .. tostring(car.handbrake) .. ' ' .. tostring(car.steeringAngle > 0) .. ' ' .. tostring(car.slip ~= nil) .. ' ' .. car.body.type"), "true true 1.0 1.0 true true true dynamic");
    EXPECT_EQ(lua("car.brake = 2 return car.brake .. ' ' .. tostring(car.valid)"), "1.0 true");
    EXPECT_EQ(lua("car:destroy() return tostring(car.valid)"), "false");
    EXPECT_NE(lua("return car.speed").find("The vehicle was destroyed."), std::string::npos);
    EXPECT_NE(lua("physics2d.newTopDownVehicle(space, {grip = 0})").find("A top-down vehicle needs"), std::string::npos);
}

} // namespace haylen
