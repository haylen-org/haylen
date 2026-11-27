#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::lighting2d {

TEST(Lighting2DLuaTest, FlickersFromLua) {
    test::LuaFixture fixture;
    fixture.runLua("lighting2d = require('haylen.lighting2d')");

    EXPECT_EQ(fixture.lua("local f = lighting2d.flicker(1.25, {speed = 6, amount = 0.1, seed = 4}) return f > 0.85 and f < 1.15 and lighting2d.flicker(0) > 0"), "true");
    EXPECT_EQ(fixture.lua("return lighting2d.flicker(2, {amount = 0}) == 1"), "true");
    EXPECT_NE(fixture.lua("lighting2d.flicker(1, {wobble = 1})").find("Unknown option \"wobble\""), std::string::npos);
    EXPECT_NE(fixture.lua("lighting2d.flicker(1, 'fast')").find("error: "), std::string::npos);
}

TEST(Lighting2DLuaTest, CreatesLightsAndAsksWhereTheyReach) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        lighting2d = require('haylen.lighting2d')
        graphics = require('haylen.graphics')
        torch = lighting2d.newLight({type = 'spot', x = 10, y = 20, radius = 200, color = '#FFFF8000', intensity = 2, innerAngle = 0.4, outerAngle = 0.9, height = 30, blend = 'mix', itemMask = 3, layerMin = -2, layerMax = 4, shadows = true, shadowFilter = 'pcf5', shadowColor = '#80000040', shadowSmoothness = 1.5, shadowMask = 6})
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return torch.type .. ' ' .. torch.x .. ' ' .. torch.position.y .. ' ' .. torch.radius .. ' ' .. torch.intensity .. ' ' .. torch.blend .. ' ' .. torch.itemMask .. ' ' .. torch.layerMin .. ' ' .. torch.shadowFilter .. ' ' .. torch.shadowMask .. ' ' .. tostring(torch.shadows) .. ' ' .. tostring(torch.enabled) .. ' ' .. tostring(torch.texture)"), "spot 10.0 20.0 200.0 2.0 mix 3 -2 pcf5 6 true true nil");
    EXPECT_EQ(fixture.lua("torch.texture = graphics.whiteTexture() torch.scaleX = 2 torch.rotation = 1 return torch.texture.width .. ' ' .. torch.scaleX .. ' ' .. torch.rotation"), "1 2.0 1.0");
    EXPECT_EQ(fixture.lua("torch.texture = nil return tostring(torch.texture)"), "nil");
    EXPECT_EQ(fixture.lua("return tostring(torch:affects(1, 0)) .. ' ' .. tostring(torch:affects(4, 0)) .. ' ' .. tostring(torch:affects(1, 5))"), "true false false");

    // A white point light adds its falloff over the ambient color, and light behind an occluder of its shadow mask is blocked.
    fixture.runLua("lamp = lighting2d.newLight({x = 0, y = 0, radius = 100, shadows = true})");
    EXPECT_EQ(fixture.lua("return lamp:strengthAt(0, 0) .. ' ' .. lamp:strengthAt(200, 0)"), "1.0 0.0");
    EXPECT_EQ(fixture.lua("local c = lamp:apply('#FF202020', 0, 0) return string.format('%.3f', c.r)"), "1.125");
    EXPECT_EQ(fixture.lua("local c = lighting2d.illuminate('#FF000000', {lamp, {x = 0, y = 0, radius = 100, intensity = 0.5, blend = 'subtract'}}, 0, 0) return c.g"), "0.5");
    EXPECT_EQ(fixture.lua("local c = lighting2d.illuminate('#FF000000', {lamp}, 0, 0, {lightMask = 2}) return c.b"), "0.0");
    fixture.runLua("wall = lighting2d.newOccluder({points = {50, -20, 50, 20}, closed = false})");
    EXPECT_EQ(fixture.lua("return tostring(lamp:shadowedAt(80, 0, {wall})) .. ' ' .. tostring(lamp:shadowedAt(20, 0, {wall})) .. ' ' .. tostring(lamp:shadowedAt(80, 0, {{points = {{50, -20}, {50, 20}}, closed = false, mask = 2}}))"), "true false false");
    EXPECT_NE(fixture.lua("lamp:shadowedAt(80, 0, {{points = {}, closed = false}})").find("An occluder needs at least 2 points, and 3 when it is closed."), std::string::npos);

    // The falloff is the curve of lights without a texture, from 1 at the center to 0 at the radius.
    EXPECT_EQ(fixture.lua("return lighting2d.falloff(0) .. ' ' .. lighting2d.falloff(1) .. ' ' .. tostring(lighting2d.falloff(0.5) == lamp:strengthAt(50, 0))"), "1.0 0.0 true");

    EXPECT_NE(fixture.lua("lighting2d.newLight({kind = 'spot'})").find("Unknown option \"kind\""), std::string::npos);
    EXPECT_NE(fixture.lua("lighting2d.newLight({type = 'area'})").find("unknown value 'area'"), std::string::npos);
    EXPECT_NE(fixture.lua("torch.itemMask = 300").find("integer out of range"), std::string::npos);
    EXPECT_NE(fixture.lua("lighting2d.illuminate('#FF000000', {}, 0, 0, {mask = 1})").find("Unknown option \"mask\""), std::string::npos);

    // The queries refuse a light that could never draw, instead of answering with numbers that are not finite.
    fixture.runLua("flat = lighting2d.newLight({radius = 0})");
    EXPECT_NE(fixture.lua("lighting2d.illuminate('#FF000000', {{radius = 0}}, 0, 0)").find("A light radius must be positive."), std::string::npos);
    EXPECT_NE(fixture.lua("flat:strengthAt(0, 0)").find("A light radius must be positive."), std::string::npos);
    EXPECT_NE(fixture.lua("flat:apply('#FF000000', 0, 0)").find("A light radius must be positive."), std::string::npos);
    EXPECT_NE(fixture.lua("flat:shadowedAt(0, 0, {})").find("A light radius must be positive."), std::string::npos);
}

TEST(Lighting2DLuaTest, BuildsOccludersFromTablesBodiesAndMaps) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        lighting2d = require('haylen.lighting2d')
        physics2d = require('haylen.physics2d')
        wall = lighting2d.newOccluder({points = {{0, 0}, {10, 0}, {10, 10}}, cull = 'clockwise', mask = 3, x = 5, y = 6, rotation = 0, scaleX = 2, scaleY = 1})
        world = physics2d.newWorld({gravity = {0, 0}})
        crate = world:createBody({x = 100, y = 50})
        crate:addBox(20, 20)
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return #wall.points .. ' ' .. tostring(wall.closed) .. ' ' .. wall.cull .. ' ' .. wall.mask .. ' ' .. wall.x .. ' ' .. wall.scaleX"), "3 true clockwise 3 5.0 2.0");
    EXPECT_EQ(fixture.lua("local points = wall:worldPoints() return points[2].x .. ' ' .. points[2].y"), "25.0 6.0");
    EXPECT_EQ(fixture.lua("wall.points = {1, 2, 3, 4} wall.closed = false return #wall.points .. ' ' .. wall.points[2].y"), "2 4.0");
    EXPECT_EQ(fixture.lua("local list = lighting2d.occludersFromBody(world, crate) return #list .. ' ' .. #list[1].points .. ' ' .. list[1].x .. ' ' .. tostring(list[1].closed)"), "1 4 100.0 true");

    EXPECT_NE(fixture.lua("wall.points = {1, 2, 3}").find("A flat list of points needs two numbers per point."), std::string::npos);
    EXPECT_NE(fixture.lua("lighting2d.newOccluder({points = {1, 2, 3, 'four'}})").find("A flat list of points needs two numbers per point."), std::string::npos);
    EXPECT_NE(fixture.lua("lighting2d.newOccluder({cull = 'both'})").find("unknown value 'both'"), std::string::npos);
    EXPECT_NE(fixture.lua("lighting2d.newOccluder({point = {}})").find("Unknown option \"point\""), std::string::npos);
}

} // namespace haylen::lua::lighting2d
