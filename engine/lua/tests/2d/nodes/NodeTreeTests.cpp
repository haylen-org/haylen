#include <gtest/gtest.h>

#include "support/LuaFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::nodes2d {

TEST(Nodes2DLuaTest, BuildsDrawsAndDestroysTreesFromLua) {
    test::LuaFixture fixture({{"content/units/tank.png", test::TestFiles::toText(test::TestFiles::pngImage(32, 32, 0xFFFFFFFFU))}, {"content/units/tank.prefab", R"({"name": "tank", "sprite": {"texture": "units/tank.png"}, "children": [{"name": "turret", "x": 8}]})"}});
    // clang-format off
    fixture.runLua(R"(
        assets = require('haylen.assets')
        collections = require('haylen.collections')
        graphics2d = require('haylen.graphics2d')
        nodes2d = require('haylen.nodes2d')
        tree = nodes2d.newTree()
        hull = tree:add({name = 'hull', x = 100, y = 50, rotation = math.pi / 2, sprite = graphics2d.newSprite(assets.texture('units/tank.png')), data = {health = 3}})
        gun = hull:add({name = 'gun', x = 10, children = {{name = 'tip', x = 5, text = 'Bang'}}})
        tree:update(0)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("local x, y = gun:toWorld(0, 0) return string.format('%.1f %.1f', x, y)"), "100.0 60.0");
    EXPECT_EQ(fixture.lua("local tip = tree:find('hull/gun/tip') return tip.path .. ' ' .. tip.text .. ' ' .. tip.content .. ' ' .. tostring(tip.parent == gun)"), "hull/gun/tip Bang text true");
    EXPECT_EQ(fixture.lua("return hull.data.health .. ' ' .. hull.childCount .. ' ' .. #tree:roots() .. ' ' .. tree.size"), "3 1 1 3");
    EXPECT_EQ(fixture.lua("hull.x = 200 tree:update(0) local x = gun:toWorld(0, 0) return string.format('%.1f', x)"), "200.0");

    // A prefab from the package instantiates with overrides, and a tank saves back to a description.
    EXPECT_EQ(fixture.lua("local tank = tree:instantiate(assets.load('units/tank.prefab'), {x = 40}) return tank.name .. ' ' .. tank.x .. ' ' .. tank:find('turret').x"), "tank 40.0 8.0");
    EXPECT_EQ(fixture.lua("local saved = nodes2d.save(tree:find('tank')) return saved.sprite.texture .. ' ' .. saved.children[1].name"), "units/tank.png turret");
    EXPECT_EQ(fixture.lua("local copy = tree:load({name = 'copy', children = {{name = 'part'}}}) return copy:find('part').path"), "copy/part");
    EXPECT_EQ(fixture.lua("local size = tree.size local ok = pcall(tree.load, tree, {name = 'broken', children = {{name = 'part'}, {name = 'bad', sprite = {texture = 'units/missing.png'}}}}) return tostring(ok) .. ' ' .. tostring(tree.size == size)"), "false true");

    // Destroyed listeners run before the nodes go, and the node handles turn invalid.
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local heard = {}
        local owner = {}
        gun:on('destroyed', function(node) heard[#heard + 1] = node.name end, {owner = owner})
        tree:find('hull/gun/tip'):on('destroyed', function(node) heard[#heard + 1] = node.name end)
        hull:destroy()
        return table.concat(heard, ',') .. ' ' .. tostring(gun.valid) .. ' ' .. tree.size
    )"), "tip,gun false 4");
    // clang-format on
    EXPECT_EQ(fixture.lua("local ok, message = pcall(function() return gun.x end) return message:match('The node.*')"), "The node was destroyed.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(tree.add, tree, {speed = 2}) return message:match('Unknown option.*')"), "Unknown option \"speed\".");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(tree.add, tree, {sprite = graphics2d.newSprite(assets.texture('units/tank.png')), text = 'Hi'}) return message"), "A node shows one of \"sprite\", \"text\", \"frame\" and \"particles\".");

    // Many nodes move from a float buffer in one call.
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local list = {}
        for index = 1, 10 do list[index] = tree:add({}) end
        local values = collections.newFloatBuffer(30)
        for index = 1, 10 do values[index * 3 - 2] = index values[index * 3 - 1] = -index end
        tree:writeTransforms(list, values)
        tree:update(0)
        local world = collections.newFloatBuffer(30)
        tree:readTransforms(list, world)
        return world[28] .. ' ' .. world[29]
    )"), "10.0 -10.0");
    // clang-format on
}

TEST(Nodes2DLuaTest, TweensNodesNatively) {
    test::LuaFixture fixture;
    fixture.runLua("nodes2d = require('haylen.nodes2d') tween = require('haylen.tween') tree = nodes2d.newTree() node = tree:add({}) tween.to(node, 0.5, {x = 100, color = '#FF000000'})");
    fixture.frames(60);
    EXPECT_EQ(fixture.lua("return string.format('%.1f %.1f', node.x, node.color.r)"), "100.0 0.0");
}

} // namespace haylen::lua::nodes2d
