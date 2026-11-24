#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::physics2d {

TEST(Physics2DLuaTest, SimulatesWorldsFromLua) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics2d = require('haylen.physics2d')
        world = physics2d.newWorld({gravity = {0, 980}, pixelsPerMeter = 64, subSteps = 4})
        ground = world:createBody({type = 'static', x = 0, y = 400})
        ground:addBox(1000, 20, {friction = 0.8, category = 2})
        zone = world:createBody({type = 'static', x = 0, y = 200})
        zone:addBox(200, 40, {sensor = true})
        crate = world:createBody({x = 0, y = 0, lockRotation = true, linearDamping = 0.1})
        crate:addBox(32, 32, {density = 2, restitution = 0, offsetX = 0, offsetY = 0, rotation = 0})
        crate.data = {name = 'crate'}
        events = {}
        world.onContactBegin = function(a, b, contact) events[#events + 1] = 'begin ' .. (a.data or b.data).name begun = contact end
        world.onContactEnd = function() events[#events + 1] = 'end' end
        world.onHit = function(a, b, contact) hitSpeed = contact.speed end
        world.onSensorBegin = function(sensor, visitor, shapes) events[#events + 1] = 'sensed ' .. visitor.data.name .. ' ' .. tostring(shapes.sensorShape.sensor) end
        world.onSensorEnd = function() events[#events + 1] = 'left' end
        for i = 1, 120 do world:step(1 / 60) end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return table.concat(events, ',')"), "sensed crate true,left,begin crate");
    EXPECT_EQ(fixture.lua("return math.floor(crate.y + 0.5) .. ' ' .. tostring(hitSpeed > 0) .. ' ' .. world.bodyCount .. ' ' .. world.pixelsPerMeter"), "374 true 3 64.0");
    EXPECT_EQ(fixture.lua("return crate.type .. ' ' .. tostring(crate.valid) .. ' ' .. tostring(crate.world == world) .. ' ' .. tostring(crate.lockRotation) .. ' ' .. tostring(crate.mass > 0)"), "dynamic true true true true");

    // A contact that begins tells how fast the crate landed, where, from which side and with what force, point by point, and the contacts of a body tell the same while it rests.
    EXPECT_EQ(fixture.lua("return tostring(begun.speed > 100) .. ' ' .. #begun.points .. ' ' .. tostring(begun.points[1].approachSpeed > 100) .. ' ' .. math.abs(begun.normalY) .. ' ' .. type(begun.relativeVelocityX) .. ' ' .. tostring(begun.force > 0) .. ' ' .. tostring(begun.points[1].separation < 1)"), "true 2 true 1.0 number true true");
    EXPECT_EQ(fixture.lua("local c = crate:contacts()[1] return #c.points .. ' ' .. tostring(c.force > 0) .. ' ' .. c.normalY .. ' ' .. tostring(c.speed < 20) .. ' ' .. type(c.points[2].tangentImpulse)"), "2 true 1.0 true number");

    fixture.runLua("crate.x = 50 crate.y = 100 crate.rotation = 0 crate.velocity = {10, 0} crate.angularVelocity = 0 crate.linearDamping = 0 crate.angularDamping = 0 crate.gravityScale = 0 crate.bullet = true crate.awake = true crate.enabled = true");
    EXPECT_EQ(fixture.lua("return crate.position.x .. ' ' .. crate.velocity.x .. ' ' .. crate.gravityScale .. ' ' .. tostring(crate.bullet) .. ' ' .. tostring(crate.awake) .. ' ' .. tostring(crate.enabled)"), "50.0 10.0 0.0 true true true");
    fixture.runLua("crate:applyImpulse(0, -10) crate:applyForce(1, 0) crate:applyForce(1, 0, 50, 100) crate:applyImpulse(1, 0, 50, 100) crate:applyTorque(1) crate:applyAngularImpulse(1) crate:setTransform(0, 0, 0.5) crate.type = 'kinematic'");
    EXPECT_EQ(fixture.lua("return crate.type .. ' ' .. string.format('%.1f', crate.rotation) .. ' ' .. crate.linearDamping .. ' ' .. crate.angularDamping"), "kinematic 0.5 0.0 0.0");

    EXPECT_EQ(fixture.lua("local hit = world:raycast(0, -500, 0, 1000) return hit.body == ground or hit.body == zone or hit.body == crate"), "true");
    EXPECT_EQ(fixture.lua("local hit = world:raycast(0, 300, 0, 1000, {mask = 2}) return hit.body == ground and hit.shape.category == 2 and hit.normalY < 0 and hit.fraction > 0 and hit.x == 0 and hit.y > 0 and hit.normalX == 0"), "true");
    EXPECT_EQ(fixture.lua("return tostring(world:raycast(5000, 0, 6000, 0))"), "nil");
    EXPECT_EQ(fixture.lua("return #world:raycastAll(0, -500, 0, 1000)"), "3");
    EXPECT_EQ(fixture.lua("return #world:queryRect({-10, 390, 20, 20}) .. ' ' .. #world:queryCircle(0, 200, 5) .. ' ' .. #world:queryPoint(0, 400, {category = 1, mask = 2})"), "1 1 1");

    // clang-format off
    fixture.runLua(R"(
        hinge = world:createBody({type = 'static', x = 500, y = 0})
        hinge:addCircle(4)
        bob = world:createBody({x = 600, y = 0})
        local shapes = bob:addPolygon({{0, 0}, {16, 0}, {16, 16}, {0, 16}}, {density = 1})
        bob:addCapsule(0, 0, 0, 20, 4)
        bob:addSegment(0, 0, 10, 0)
        hinge:addChain({{0, 0}, {10, 0}, {10, 10}, {0, 10}}, true)
        joint = world:createJoint('revolute', hinge, bob, {ax = 500, ay = 0, enableMotor = true, motorSpeed = 1, maxMotorTorque = 10, enableLimit = true, lower = -1, upper = 1})
        mouse = world:createJoint('mouse', hinge, bob, {bx = 600, by = 0, hertz = 5, dampingRatio = 0.7, maxMotorForce = 100})
        for i = 1, 10 do world:step(1 / 60) end
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("local slider = world:createBody({lockX = true}) slider.lockY = true slider.lockX = false local locks = tostring(slider.lockX) .. ' ' .. tostring(slider.lockY) .. ' ' .. tostring(slider.lockRotation) slider:destroy() return locks"), "false true false");
    EXPECT_EQ(fixture.lua("local coin = world:createBody({continuousThreshold = 0.2}) coin.continuousThreshold = 0.25 local threshold = coin.continuousThreshold coin:destroy() return tostring(threshold)"), "0.25");
    EXPECT_NE(fixture.lua("world:createBody({continuousThreshold = -1})").find("A physics body needs a finite continuous threshold of zero or more."), std::string::npos);
    EXPECT_EQ(fixture.lua("joint.motorSpeed = 2 mouse.target = {610, 10} return tostring(joint.valid) .. ' ' .. #bob:shapes() .. ' ' .. joint.motorSpeed .. ' ' .. math.floor(mouse.target.x + 0.5)"), "true 3 2.0 610");
    EXPECT_NE(fixture.lua("return joint.target").find("Only mouse joints have a target."), std::string::npos);
    EXPECT_NE(fixture.lua("mouse.motorSpeed = 1").find("Only revolute, prismatic, wheel and distance joints have a motor speed."), std::string::npos);
    EXPECT_EQ(fixture.lua("local s = bob:shapes()[1] s.mask = 1 s.category = 4 s.group = -3 return s.mask .. ' ' .. s.category .. ' ' .. s.group .. ' ' .. tostring(s.body == bob) .. ' ' .. tostring(s == s) .. ' ' .. tostring(s.bounds.width > 0)"), "1 4 -3 true true true");
    EXPECT_EQ(fixture.lua("local s = bob:addCircle(2, {group = 5}) return s.group .. ' ' .. hinge:shapes()[1].group"), "5 0");
    EXPECT_EQ(fixture.lua("local s = bob:shapes()[2] s:destroy() return tostring(s.valid) .. ' ' .. #bob:shapes()"), "false 3");
    EXPECT_EQ(fixture.lua("joint:destroy() return tostring(joint.valid)"), "false");
    EXPECT_EQ(fixture.lua("world.gravity = {0, 10} return world.gravity.y"), "10.0");

    fixture.runLua("require('haylen.scene').push({render = function() require('haylen.graphics2d').beginWorld(require('haylen.graphics2d').newCamera()) world:debugDraw({layer = 5}) end})");
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    EXPECT_EQ(fixture.lua("crate:destroy() crate:destroy() return tostring(crate.valid) .. ' ' .. world.bodyCount"), "false 4");
    EXPECT_NE(fixture.lua("return crate.x").find("The body was destroyed."), std::string::npos);
    EXPECT_NE(fixture.lua("world:createBody({kind = 'dynamic'})").find("Unknown option \"kind\""), std::string::npos);
    EXPECT_NE(fixture.lua("bob:addBox(10, 10, {bounce = 1})").find("Unknown option \"bounce\""), std::string::npos);
    EXPECT_NE(fixture.lua("world:createJoint('rope', hinge, bob)").find("unknown value 'rope'"), std::string::npos);
    EXPECT_NE(fixture.lua("world:createJoint('weld', hinge, bob, {speed = 1})").find("Unknown option \"speed\""), std::string::npos);
    EXPECT_NE(fixture.lua("world:raycast(0, 0, 1, 1, {layer = 1})").find("Unknown option \"layer\""), std::string::npos);
    EXPECT_NE(fixture.lua("physics2d.newWorld({pixelsPerMeter = 0})").find("positive pixels per meter"), std::string::npos);
    EXPECT_NE(fixture.lua("world.onHit = 5").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("bob:addBox(0, 10)").find("positive size"), std::string::npos);
    EXPECT_NE(fixture.lua("bob:addBox(10, 10, {density = -1})").find("A physics shape needs a finite density, friction, restitution and rolling resistance of zero or more."), std::string::npos);
    EXPECT_NE(fixture.lua("bob:addSegment(0, 0, 0, 0)").find("A physics segment needs ends more than 0.005 meters apart."), std::string::npos);
    EXPECT_NE(fixture.lua("bob.linearDamping = -1").find("A physics body needs a finite damping of zero or more."), std::string::npos);
    EXPECT_NE(fixture.lua("world:createJoint('distance', hinge, bob)").find("A distance joint needs a length of at least 0.005 meters."), std::string::npos);
    EXPECT_NE(fixture.lua("world:createJoint('revolute', hinge, bob, {lower = 1, upper = 0})").find("A revolute joint needs a lower limit"), std::string::npos);
    EXPECT_NE(fixture.lua("world:queryRect({0, 0, -10, 10})").find("A rectangle query needs a width and height of zero or more."), std::string::npos);
    EXPECT_EQ(fixture.lua("return #bob:shapes()"), "3");

    fixture.runLua("world.onHit = nil world.onContactBegin = function() error('contact failed') end local b = world:createBody({x = 0, y = 384}) b:addBox(10, 10)");
    EXPECT_NE(fixture.lua("for i = 1, 60 do world:step(1 / 60) end").find("contact failed"), std::string::npos);
    EXPECT_EQ(fixture.lua("return type(world.onContactBegin)"), "function");
}

TEST(Physics2DLuaTest, DeliversEveryEventWhenACallbackStepsTheWorld) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics2d = require('haylen.physics2d')
        world = physics2d.newWorld({gravity = {0, 0}})
        local ground = world:createBody({type = 'static', x = 0, y = 0})
        ground:addBox(100, 20)
        local crate = world:createBody({x = 0, y = -12})
        crate:addBox(10, 10)
        local zone = world:createBody({type = 'static', x = 0, y = -28})
        zone:addBox(40, 24, {sensor = true})
        events = {}
        world.onContactBegin = function()
            events[#events + 1] = 'contact'
            if not nested then
                nested = true
                world:step(0)
            end
        end
        world.onSensorBegin = function() events[#events + 1] = 'sensor' end
    )");
    // clang-format on

    // The callback steps the world again, which replaces its event lists, yet the events of the outer step still arrive.
    EXPECT_EQ(fixture.lua("world:step(1 / 60) return table.concat(events, ',')"), "contact,sensor");
}

// A world that `destroy` ended lets its callbacks and the data of its bodies go, and its world, bodies, shapes and joints fail with sentences that name what was destroyed.
TEST(Physics2DLuaTest, DestroysWorldsFromLua) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics2d = require('haylen.physics2d')
        world = physics2d.newWorld({gravity = {0, 0}})
        crate = world:createBody({x = 0, y = 0})
        box = crate:addBox(10, 10)
        hinge = world:createJoint('weld', crate, world:createBody({x = 20, y = 0}))
        crate.data = {name = 'Crate'}
        world.onContactBegin = function() end
        world:destroy()
        world:destroy()
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return tostring(world.destroyed) .. ' ' .. tostring(crate.valid) .. ' ' .. tostring(box.valid) .. ' ' .. tostring(hinge.valid)"), "true false false false");
    EXPECT_NE(fixture.lua("world:step(1 / 60)").find("The physics world was destroyed."), std::string::npos);
    EXPECT_NE(fixture.lua("return world.bodyCount").find("The physics world was destroyed."), std::string::npos);
    EXPECT_NE(fixture.lua("return world.onContactBegin").find("The physics world was destroyed."), std::string::npos);
    EXPECT_NE(fixture.lua("world:raycast(0, 0, 10, 0)").find("The physics world was destroyed."), std::string::npos);
    EXPECT_NE(fixture.lua("physics2d.newVehicle(world, {x = 0, y = 0})").find("The physics world was destroyed."), std::string::npos);
    EXPECT_NE(fixture.lua("return crate.x").find("The physics world of the body was destroyed."), std::string::npos);
    EXPECT_NE(fixture.lua("return box.friction").find("The physics world of the shape was destroyed."), std::string::npos);
    EXPECT_NE(fixture.lua("return hinge.type").find("The physics world of the joint was destroyed."), std::string::npos);
    EXPECT_EQ(fixture.lua("crate:destroy() return tostring(physics2d.newWorld().destroyed)"), "false");
}

} // namespace haylen::lua::physics2d
