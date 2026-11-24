#include <gtest/gtest.h>

#include <string>

#include "haylen/2d/graphics/Renderer.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::physics2d {

TEST(WorldRaycastLuaTest, CastsRaysAndShapesFromLua) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics2d = require('haylen.physics2d')
        world = physics2d.newWorld({gravity = {0, 0}})
        posts = {}
        for index = 1, 3 do
            posts[index] = world:createBody({type = 'static', x = index * 100, y = 0})
            posts[index]:addBox(10, 100, {category = index == 2 and 2 or 1, group = index == 3 and -3 or 0})
            posts[index].data = {name = 'post' .. index}
        end
        mirror = world:createBody({type = 'static', x = 400, y = 0})
        mirror:addBox(10, 1000, {category = 4})
        mirror.data = {name = 'right'}
        leftMirror = world:createBody({type = 'static', x = -400, y = 0})
        leftMirror:addBox(10, 1000, {category = 4})
        leftMirror.data = {name = 'left'}
        function names(hits)
            local list = {}
            for _, hit in ipairs(hits) do list[#list + 1] = hit and hit.body.data.name or 'miss' end
            return table.concat(list, ',')
        end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("local hit = world:raycast(0, 0, 1000, 0) return hit.body.data.name .. ' ' .. math.floor(hit.x + 0.5) .. ' ' .. hit.normalX .. ' ' .. math.floor(hit.distance + 0.5) .. ' ' .. tostring(hit.shape.valid)"), "post1 95 -1.0 95 true");
    EXPECT_EQ(fixture.lua("return names(world:raycastAll(0, 0, 1000, 0)) .. ' / ' .. names(world:raycastAll(0, 0, 1000, 0, {limit = 2}))"), "post1,post2,post3,right / post1,post2");
    EXPECT_EQ(fixture.lua("return world:raycast(0, 0, 1000, 0, {mask = 2}).body.data.name .. ' ' .. world:raycast(150, 0, 1000, 0, {mask = 5, group = -3}).body.data.name"), "post2 right");
    EXPECT_EQ(fixture.lua("return world:raycast(0, 0, 1000, 0, {accept = function(hit) return hit.body.data.name == 'post3' end}).body.data.name"), "post3");
    EXPECT_EQ(fixture.lua("return names(world:raycastAll(0, 0, 1000, 0, {accept = function(hit) return hit.x > 150 end, limit = 1}))"), "post2");
    EXPECT_EQ(fixture.lua("return tostring(world:lineOfSight(0, 200, 300, 200)) .. tostring(world:lineOfSight(0, 0, 300, 0)) .. tostring(world:lineOfSight(0, 0, 300, 0, {mask = 4}))"), "truefalsetrue");

    EXPECT_EQ(fixture.lua("return math.floor(world:castCircle(0, 0, 10, 500, 0).distance + 0.5) .. ' ' .. math.floor(world:castBox(0, 0, 40, 20, 0, 500, 0).distance + 0.5)"), "85 75");
    EXPECT_EQ(fixture.lua("return math.floor(world:castCapsule(-20, 0, 20, 0, 5, 500, 0).distance + 0.5) .. ' ' .. math.floor(world:castPolygon({{0, -10}, {30, 0}, {0, 10}}, 500, 0).distance + 0.5)"), "70 65");
    EXPECT_EQ(fixture.lua("return tostring(world:castCircle(0, 200, 10, 300, 0)) .. ' ' .. world:castCircle(0, 0, 10, 500, 0, {mask = 2}).body.data.name"), "nil post2");

    EXPECT_EQ(fixture.lua("local hits, x, y = world:bounceRay(0, 50, 1, 0, 5000, 2, {mask = 4}) return names(hits) .. ' ' .. math.floor(hits[2].distance + 0.5) .. ' ' .. math.floor(x + 0.5)"), "right,left,right 1185 395");
    EXPECT_EQ(fixture.lua("return names(world:rayFan(0, 0, 0, math.pi, 5, 1000, {mask = 4}))"), "miss,right,right,right,miss");
    EXPECT_NE(fixture.lua("world:rayFan(0, 0, 0, 1, 3, math.huge)").find("the length must be finite"), std::string::npos);

    // clang-format off
    fixture.runLua(R"(
        batch = physics2d.newRayBatch(3)
        batch:setRay(1, 0, 0, 1000, 0)
        batch:setRay(2, 0, 300, 1000, 300)
        batch:setRay(3, 0, 0, -1000, 0)
        world:raycastBatch(batch, {mask = 5})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("local hit, x, y, nx, ny, fraction = batch:hit(1) return tostring(hit) .. ' ' .. math.floor(x + 0.5) .. ' ' .. nx .. ' ' .. string.format('%.3f', fraction) .. ' ' .. tostring(batch:hit(3)) .. ' ' .. batch.size"), "true 95 -1.0 0.095 true 3");
    EXPECT_EQ(fixture.lua("return batch:body(1).data.name .. ' ' .. batch:body(2).data.name .. ' ' .. tostring(batch:shape(1).valid) .. ' ' .. select(3, batch:ray(2))"), "post1 right true 1000.0");
    EXPECT_EQ(fixture.lua("batch.size = 1 return batch.size"), "1");
    EXPECT_EQ(fixture.lua("local empty = physics2d.newRayBatch() local before = empty.size empty.size = 2 return before .. ' ' .. empty.size .. ' ' .. select(3, empty:ray(2)) .. ' ' .. tostring(empty:hit(2))"), "0 2 0.0 false");

    fixture.runLua("world.debugRays = true world:raycast(0, 0, 1000, 0) world:raycastAll(0, 20, 1000, 20)");
    EXPECT_EQ(fixture.lua("return tostring(world.debugRays)"), "true");
    EXPECT_EQ(fixture.lua("world.debugRays = false return tostring(world.debugRays)"), "false");

    EXPECT_NE(fixture.lua("world:raycastBatch(batch, {accept = function() return true end})").find("no \"accept\" function"), std::string::npos);
    EXPECT_NE(fixture.lua("world:raycast(0, 0, 1, 0, {limit = 2})").find("Unknown option \"limit\""), std::string::npos);
    EXPECT_NE(fixture.lua("world:bounceRay(0, 0, 0, 0, 10, 1)").find("the direction must not be zero"), std::string::npos);
    EXPECT_NE(fixture.lua("world:raycast(0, 0, 1000, 0, {accept = function() error('filter failed') end})").find("filter failed"), std::string::npos);
    EXPECT_NE(fixture.lua("batch:hit(9)").find("outside the batch"), std::string::npos);

    // Recorded casts and single rays draw inside a world canvas, and screen points pick the shapes under them.
    // clang-format off
    fixture.runLua(R"(
        local graphics2d = require('haylen.graphics2d')
        camera = graphics2d.newCamera()
        local screen = require('haylen.viewport').visibleRect()
        cx, cy = screen.x + screen.width / 2, screen.y + screen.height / 2
        world.debugRays = true
        require('haylen.scene').push({
            update = function() world:rayFan(0, 0, 0, 1, 5, 500) end,
            render = function()
                graphics2d.beginWorld(camera)
                world:debugDrawRays({layer = 5})
                physics2d.drawRay(0, 0, 100, 0, world:raycast(0, 0, 1000, 0))
                physics2d.drawRay(0, 0, 100, 0, false, {layer = 6})
            end,
        })
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    const haylen::graphics2d::Renderer::Stats& stats = fixture.engine().getRenderer2D().getStats();
    EXPECT_GT(stats.instances, stats.sprites);

    EXPECT_EQ(fixture.lua("camera.position = {200, 0} local picked = world:pick(camera, cx, cy) return #picked .. ' ' .. picked[1].body.data.name"), "1 post2");
    EXPECT_EQ(fixture.lua("return #world:pick(camera, cx, cy - 200) .. ' ' .. #world:pick(camera, cx, cy, {mask = 1})"), "0 0");
    EXPECT_NE(fixture.lua("world:pick(camera, cx, cy, {group = 1})").find("Unknown option \"group\""), std::string::npos);
}

} // namespace haylen::lua::physics2d
