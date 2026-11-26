#include <gtest/gtest.h>

#include <memory>

#include "haylen/core/Engine.hpp"
#include "haylen/plugins/Nodes3DPlugin.hpp"
#include "support/LuaFixture.hpp"
#include "support/SpinnerComponent3D.hpp"

namespace haylen::lua::nodes3d {

TEST(Nodes3DLuaTest, BuildsFindsAndPlacesNodesFromLua) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        m = require('haylen.math')
        nodes3d = require('haylen.nodes3d')
        round = function(value) return string.format('%.1f', math.floor(value * 10 + 0.5) / 10) end
        place = function(v) return round(v.x) .. ' ' .. round(v.y) .. ' ' .. round(v.z) end
        tree = nodes3d.newTree()
        tank = tree:add({name = 'tank', position = m.vec3(10, 0, -4), rotation = m.fromAxisAngle(m.up(), math.pi / 2), scale = m.vec3(2, 2, 2), tags = {'vehicle'}, data = {health = 3}})
        gun = tank:add({name = 'gun', position = m.vec3(1, 0, 0), children = {{name = 'tip', position = m.vec3(0, 0, -1), layers = 4, renderOrder = 2}}})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return place(gun.worldPosition)"), "10.0 0.0 -6.0");
    EXPECT_EQ(fixture.lua("local tip = tree:find('tank/gun/tip') return tip.path .. ' ' .. tip.layers .. ' ' .. tip.renderOrder .. ' ' .. tostring(tip.parent == gun)"), "tank/gun/tip 4 2 true");
    EXPECT_EQ(fixture.lua("return tank.data.health .. ' ' .. tank.childCount .. ' ' .. #tree:roots() .. ' ' .. tree.size .. ' ' .. tank.tags[1]"), "3 1 1 3 vehicle");
    EXPECT_EQ(fixture.lua("tank.x = 20 return round(gun:toWorld(m.vec3(0, 0, 0)).x)"), "20.0");
    EXPECT_EQ(fixture.lua("return place(gun:toLocal(gun:toWorld(m.vec3(1, 2, 3))))"), "1.0 2.0 3.0");
    EXPECT_EQ(fixture.lua("return #tree:findTagged('vehicle') .. ' ' .. tostring(tank:hasTag('vehicle'))"), "1 true");

    // Parts of the parent that a node does not inherit, a move that keeps the world place and a look at a target.
    EXPECT_EQ(fixture.lua("gun.inheritPosition = false local p = gun.worldPosition gun.inheritPosition = true return place(p)"), "1.0 0.0 0.0");
    EXPECT_EQ(fixture.lua("local before = gun.worldPosition gun:setParent(nil, {keepWorld = true}) return tostring(gun.parent) .. ' ' .. place(gun.worldPosition - before)"), "nil 0.0 0.0 0.0");
    EXPECT_EQ(fixture.lua("gun:lookAt(m.vec3(100, 0, -6)) return place(gun.worldMatrix:applyVector(m.forward()):normalized())"), "1.0 0.0 0.0");
    EXPECT_EQ(fixture.lua("gun.worldPosition = m.vec3(0, 5, 0) return round(gun.y)"), "5.0");

    // A clone copies the state and the data table of every node, and the copy of the table is a table of its own.
    EXPECT_EQ(fixture.lua("local copy = tank:clone() copy.data.health = 1 return copy.name .. ' ' .. copy.data.health .. ' ' .. tank.data.health .. ' ' .. tostring(copy ~= tank) .. ' ' .. #tree:findTagged('vehicle')"), "tank 1 3 true 2");
    EXPECT_EQ(fixture.lua("local copy = gun:clone({parent = tank}) return tostring(copy.parent == tank) .. ' ' .. copy.path"), "true tank/gun");

    EXPECT_EQ(fixture.lua("local ok, message = pcall(tree.add, tree, {speed = 2}) return message:match('Unknown option.*')"), "Unknown option \"speed\".");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() tank.parent = tank end) return message:match('A node.*')"), "A node cannot move under itself or under one of its descendants.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(tank.on, tank, 'moved', function() end) return message:match('Unknown node.*')"), "Unknown node event \"moved\". Nodes report \"destroyed\", \"parentChanged\", \"childAdded\", \"childRemoved\" and \"enabledChanged\".");
}

TEST(Nodes3DLuaTest, CallsListenersWithTheirOwnersAndRefusesGoneNodes) {
    test::LuaFixture fixture;
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local nodes3d = require('haylen.nodes3d')
        local tree = nodes3d.newTree()
        local root = tree:add({name = 'root'})
        local other = tree:add({name = 'other'})
        local child = root:add({name = 'child'})
        local leaf = child:add({name = 'leaf', data = {coins = 2}})
        local heard = {}
        local owner = {}
        for _, event in ipairs({'parentChanged', 'childAdded', 'childRemoved', 'enabledChanged', 'destroyed'}) do
            for _, node in ipairs({root, other, child, leaf}) do
                node:on(event, function(subject) heard[#heard + 1] = node.name .. ' ' .. event .. ' ' .. subject.name end, {owner = owner})
            end
        end
        child.parent = other
        other.enabled = false
        local coins = leaf.data.coins
        child:destroy()
        local ok, message = pcall(function() return leaf.x end)
        return table.concat(heard, ',') .. ' | ' .. coins .. ' ' .. tostring(leaf.valid) .. ' ' .. message:match('The node.*') .. ' ' .. tree.size
    )"), "child parentChanged child,root childRemoved child,other childAdded child,other enabledChanged other,child enabledChanged child,leaf enabledChanged leaf,child childRemoved leaf,leaf destroyed leaf,other childRemoved child,child destroyed child | 2 false The node was destroyed. 2");
    // clang-format on
}

TEST(Nodes3DLuaTest, MovesManyNodesAndUpdatesOnTheJobSystem) {
    test::LuaFixture fixture;
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local collections = require('haylen.collections')
        local m = require('haylen.math')
        local nodes3d = require('haylen.nodes3d')
        local tree = nodes3d.newTree()
        local parent = tree:add({position = m.vec3(0, 10, 0)})
        local list = {}
        for index = 1, 10 do list[index] = parent:add({}) end
        local values = collections.newFloatBuffer(71)
        for index = 1, 10 do
            values[index * 7 - 5] = index
            values[index * 7 + 1] = 1
        end
        tree:writeTransforms(list, values, 2)
        tree:update()
        local world = collections.newFloatBuffer(70)
        tree:readTransforms(list, world)
        local ok, message = pcall(tree.readTransforms, tree, list, collections.newFloatBuffer(69))
        return world[64] .. ' ' .. world[65] .. ' ' .. world[70] .. ' ' .. tree.updatedCount .. ' ' .. message:match('Node3D transforms.*')
    )"), "10.0 10.0 1.0 11 Node3D transforms take seven floats for each node.");
    // clang-format on
}

TEST(Nodes3DLuaTest, TweensNodesNatively) {
    test::LuaFixture fixture;
    fixture.runLua("m = require('haylen.math') nodes3d = require('haylen.nodes3d') tween = require('haylen.tween') tree = nodes3d.newTree() node = tree:add({}) tween.to(node, 0.5, {x = 100, rotation = m.fromAxisAngle(m.up(), math.pi / 2)})");
    fixture.frames(60);
    EXPECT_EQ(fixture.lua("local r = node.worldRotation return string.format('%.1f %.3f %.3f', node.x, r.y, r.w)"), "100.0 0.707 0.707");
}

TEST(Nodes3DLuaTest, AddsReadsAndSavesComponentsOfRegisteredTypes) {
    test::LuaFixture fixture;
    fixture.engine().getPlugin<haylen::plugins::Nodes3DPlugin>().getComponentTypes().add(std::make_shared<test::SpinnerComponent3D>());
    // clang-format off
    fixture.runLua(R"(
        m = require('haylen.math')
        nodes3d = require('haylen.nodes3d')
        tree = nodes3d.newTree()
        wheel = tree:add({name = 'wheel'})
        spin = wheel:addComponent('spinner', {speed = 6, axis = m.vec3(0, 1, 0)})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("local t = nodes3d.componentTypes()[1] return t.name .. ' ' .. t.phases[1] .. ' ' .. t.properties[2].name .. ' ' .. t.properties[2].kind .. ' ' .. t.properties[2].default.y"), "spinner update axis vec3 1.0");
    EXPECT_EQ(fixture.lua("local axis = spin:get('axis') return spin.type .. ' ' .. tostring(spin.node == wheel) .. ' ' .. spin:get('speed') .. ' ' .. axis.y .. ' ' .. tree.componentCount"), "spinner true 6.0 1.0 1");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return string.format('%.2f', wheel.rotation:angle())"), "0.30");

    // A tree whose components run only while the game is paused rests them while it runs.
    EXPECT_EQ(fixture.lua("tree.processMode = 'whenPaused' return tree.processMode"), "whenPaused");
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("tree.processMode = 'inherit' return string.format('%.2f', wheel.rotation:angle())"), "0.30");

    EXPECT_EQ(fixture.lua("local copy = wheel:clone() return #copy:components() .. ' ' .. copy:component('spinner'):get('speed') .. ' ' .. tree.componentCount"), "1 6.0 2");
    EXPECT_EQ(fixture.lua("local saved = wheel:saveComponents() local other = tree:add({}) other:loadComponents(saved) return other:component('spinner'):get('speed') .. ' ' .. saved.spinner.axis[2]"), "6.0 1.0");
    EXPECT_EQ(fixture.lua("spin:set('speed', 2) wheel:removeComponent('spinner') return tostring(spin.valid) .. ' ' .. tostring(wheel:component('spinner'))"), "false nil");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(wheel.addComponent, wheel, 'rocket') return message:match('No component.*')"), "No component type is named \"rocket\". Register it in the component types of the 3D nodes plugin.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(wheel.addComponent, wheel, 'spinner', {speed = 'fast'}) return message:match('A spinner.*')"), "A spinner takes a number \"speed\" and a vector \"axis\".");
}

} // namespace haylen::lua::nodes3d
