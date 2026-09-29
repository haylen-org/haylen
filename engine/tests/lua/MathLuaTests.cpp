#include <gtest/gtest.h>

#include <string>

#include "support/EngineFixture.hpp"

namespace haylen::math {

class MathLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("m = require('haylen.math')");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::EngineFixture fixture;
};

TEST_F(MathLuaTest, WorksWithVectors) {
    EXPECT_EQ(lua("local v = m.vec2(3, 4) return v:length() .. ' ' .. v:lengthSquared()"), "5.0 25.0");
    EXPECT_EQ(lua("return tostring(m.vec2(1, 2) + m.vec2(3, 4))"), "Vec2(4.0, 6.0)");
    EXPECT_EQ(lua("local v = (m.vec2(4, 2) - {x = 1, y = 1}) * 2 / 2 return v.x .. ',' .. v.y"), "3.0,1.0");
    EXPECT_EQ(lua("local v = 2 * m.vec2(1, 2) * m.vec2(2, 2) / m.vec2(2, 4) return v.x .. ',' .. v.y"), "2.0,2.0");
    EXPECT_EQ(lua("local v = -m.vec2(1, -2) return v:unpack()"), "-1.0");
    EXPECT_EQ(lua("return m.vec2(1, 2) == m.vec2(1, 2) and m.vec2(1, 2) ~= m.vec2(2, 1)"), "true");
    EXPECT_EQ(lua("return m.vec2(10, 0):normalized().x .. ' ' .. m.vec2(1, 0):dot({0, 1}) .. ' ' .. m.vec2(1, 0):cross({0, 1})"), "1.0 0.0 1.0");
    EXPECT_EQ(lua("return m.vec2(0, 0):distance({3, 4}) .. ' ' .. m.vec2(0, 0):lerp({10, 0}, 0.5).x"), "5.0 5.0");
    EXPECT_EQ(lua("local v = m.vec2(1, 0):rotated(m.pi / 2) return m.approximately(v.x, 0) and m.approximately(v.y, 1)"), "true");
    EXPECT_EQ(lua("return m.vec2(0, 1):angle() == m.pi / 2 and m.vec2(1, 0):perpendicular().y == 1"), "true");
    EXPECT_EQ(lua("return m.vec2(30, 40):clampedLength(5):length()"), "5.0");
    EXPECT_EQ(lua("return m.vec2().x"), "0.0");
    EXPECT_NE(lua("return m.vec2(1, 1) + 'x'").find("error: "), std::string::npos);

    EXPECT_EQ(lua("local v = m.fromAngle(m.halfPi, 2) return m.approximately(v.x, 0) and m.approximately(v.y, 2) and m.fromAngle(0).x == 1"), "true");
    EXPECT_EQ(lua("return m.vec2(1, 1):distanceSquared({4, 5})"), "25.0");
    EXPECT_EQ(lua("return tostring(m.vec2(1, 5):min({2, 3})) .. ' ' .. tostring(m.vec2(1, 5):max({2, 3}))"), "Vec2(1.0, 3.0) Vec2(2.0, 5.0)");
    EXPECT_EQ(lua("return tostring(m.vec2(1.7, -1.2):floor()) .. ' ' .. tostring(m.vec2(1.5, -1.4):round())"), "Vec2(1.0, -2.0) Vec2(2.0, -1.0)");
    EXPECT_EQ(lua("return tostring(m.vec2():isZero()) .. ' ' .. tostring(m.vec2(0, 1):isZero())"), "true false");
}

TEST_F(MathLuaTest, WorksWithRectangles) {
    lua("r = m.rect(10, 20, 100, 50)");
    EXPECT_EQ(lua("return r:left() .. ' ' .. r:right() .. ' ' .. r:top() .. ' ' .. r:bottom()"), "10.0 110.0 20.0 70.0");
    EXPECT_EQ(lua("return r:center().x .. ' ' .. r:size().y .. ' ' .. tostring(r:empty())"), "60.0 50.0 false");
    EXPECT_EQ(lua("return r:contains({x = 15, y = 25}) and r:contains(m.vec2(20, 30)) and r:contains({x = 20, y = 30, width = 5, height = 5})"), "true");
    EXPECT_EQ(lua("return r:contains(m.rect(0, 0, 500, 500))"), "false");
    EXPECT_EQ(lua("return r:intersects({x = 100, y = 60, width = 50, height = 50}) and r:intersection(m.rect(100, 60, 50, 50)).width == 10"), "true");
    EXPECT_EQ(lua("return r:merged(m.rect(0, 0, 1, 1)).x .. ' ' .. r:expanded(5).width"), "0.0 110.0");
    EXPECT_EQ(lua("return r == m.rect(10, 20, 100, 50)"), "true");
    EXPECT_EQ(lua("return tostring(m.rect(1, 2, 3, 4))"), "Rect(1.0, 2.0, 3.0, 4.0)");
    EXPECT_NE(lua("return r:contains('x')").find("error: "), std::string::npos);

    EXPECT_EQ(lua("return tostring(m.fromMinMax({10, 20}, {110, 70})) .. ' ' .. tostring(m.fromCenter({60, 45}, {100, 50}))"), "Rect(10.0, 20.0, 100.0, 50.0) Rect(10.0, 20.0, 100.0, 50.0)");
    EXPECT_EQ(lua("return tostring(r:position()) .. ' ' .. tostring(r:min()) .. ' ' .. tostring(r:max()) .. ' ' .. r:area()"), "Vec2(10.0, 20.0) Vec2(10.0, 20.0) Vec2(110.0, 70.0) 5000.0");
    EXPECT_EQ(lua("return tostring(r:inset(5)) .. ' ' .. tostring(r:inset({left = 10, top = 0, right = 20, bottom = 5}))"), "Rect(15.0, 25.0, 90.0, 40.0) Rect(20.0, 20.0, 70.0, 45.0)");
    EXPECT_EQ(lua("return tostring(r:inset({60, 30, 60, 30}))"), "Rect(70.0, 50.0, 0.0, 0.0)");
    EXPECT_EQ(lua("return tostring(r:translated({-10, 5})) .. ' ' .. tostring(r:clamp({500, -3}))"), "Rect(0.0, 25.0, 100.0, 50.0) Vec2(110.0, 20.0)");
    EXPECT_NE(lua("return r:inset({1, 2, 3})").find("Expected a number in field 'bottom'."), std::string::npos);
    EXPECT_NE(lua("return r:inset('wide')").find("bad argument #1 to 'inset' (number or table with left, top, right and bottom expected, got string)"), std::string::npos);
}

TEST_F(MathLuaTest, WorksWithColors) {
    EXPECT_EQ(lua("return m.color('#FF8000'):toHex()"), "#FFFF8000");
    EXPECT_EQ(lua("return tostring(m.color(1, 0, 0, 0.5))"), "#80FF0000");
    EXPECT_EQ(lua("return m.color():toHex()"), "#FFFFFFFF");
    EXPECT_EQ(lua("return m.color(1, 1, 1):withAlpha(0):toHex()"), "#00FFFFFF");
    EXPECT_EQ(lua("return m.color(0, 0, 0):lerp({1, 1, 1, 1}, 0.5).r"), "0.5");
    EXPECT_EQ(lua("return (m.color(1, 0.5, 1) * m.color(0.5, 1, 1)).g"), "0.5");
    EXPECT_EQ(lua("return m.fromHsv(1 / 3, 1, 1):toHex()"), "#FF00FF00");
    EXPECT_EQ(lua("return m.color('#00000000') == m.color(0, 0, 0, 0)"), "true");
    EXPECT_NE(lua("return m.color('#GG0000')").find("error: "), std::string::npos);

    EXPECT_EQ(lua("return m.white():toHex() .. ' ' .. m.black():toHex() .. ' ' .. m.transparent():toHex()"), "#FFFFFFFF #FF000000 #00000000");
    EXPECT_EQ(lua("return m.fromRgba8(255, 128, 0):toHex() .. ' ' .. m.fromRgba8(0, 0, 255, 64):toHex()"), "#FFFF8000 #400000FF");
    EXPECT_EQ(lua("return m.fromHex(0xFF800080):toHex()"), "#80FF8000");
    EXPECT_EQ(lua("local c = m.color(1, 0.5, 0, 0.5):premultiplied() return c.r .. ' ' .. c.g .. ' ' .. c.a"), "0.5 0.25 0.5");
    EXPECT_NE(lua("return m.fromRgba8(256, 0, 0)").find("bad argument #1 to 'fromRgba8' (expected an integer from 0 to 255)"), std::string::npos);
    EXPECT_NE(lua("return m.fromHex(-1)").find("bad argument #1 to 'fromHex' (expected an integer from 0 to 0xFFFFFFFF)"), std::string::npos);
}

TEST_F(MathLuaTest, ComposesTransforms) {
    lua("t = m.transform({10, 0}, m.pi / 2, {2, 2})");
    EXPECT_EQ(lua("local p = t:apply({1, 0}) return m.approximately(p.x, 10) and m.approximately(p.y, 2)"), "true");
    EXPECT_EQ(lua("local v = t:applyVector({1, 0}) return m.approximately(v.x, 0) and m.approximately(v.y, 2)"), "true");
    EXPECT_EQ(lua("local p = t:inverse():apply(t:apply({3, 4})) return m.approximately(p.x, 3) and m.approximately(p.y, 4)"), "true");
    EXPECT_EQ(lua("local p = (t * m.transform({1, 1})):apply({0, 0}) return m.approximately(p.x, 8) and m.approximately(p.y, 2)"), "true");

    EXPECT_EQ(lua("return m.approximately(t.a, 0) and m.approximately(t.b, 2) and m.approximately(t.c, -2) and m.approximately(t.d, 0) and t.tx == 10 and t.ty == 0"), "true");
    EXPECT_EQ(lua("local u = m.identity() u.tx, u.d = 5, 3 return tostring(u:apply({1, 1})) .. ' ' .. u:determinant()"), "Vec2(6.0, 3.0) 3.0");
    EXPECT_EQ(lua("return tostring(m.translation({3, 4}):translation()) .. ' ' .. tostring(m.scaling({2, 3}):apply({1, 1}))"), "Vec2(3.0, 4.0) Vec2(2.0, 3.0)");
    EXPECT_EQ(lua("local p = m.rotation(m.halfPi):apply({1, 0}) return m.approximately(p.x, 0) and m.approximately(p.y, 1) and m.approximately(t:determinant(), 4)"), "true");
    EXPECT_EQ(lua("local s = m.transform({0, 0}, 0, {1, 1}, {m.pi / 4, 0}) local p = s:apply({0, 1}) return m.approximately(p.x, -math.sqrt(0.5)) and m.approximately(s:apply({1, 0}).y, 0)"), "true");
}

TEST_F(MathLuaTest, GeneratesDeterministicRandomValuesAndNoise) {
    EXPECT_EQ(lua("local a, b = m.random(7), m.random(7) return a:nextFloat() == b:nextFloat() and a:integer(1, 6) == b:integer(1, 6)"), "true");
    EXPECT_EQ(lua("local r = m.random(1) for i = 1, 100 do local v = r:range(2, 3) if v < 2 or v > 3 then return false end end return true"), "true");
    EXPECT_EQ(lua("local r = m.random(1) return r:chance(1) and not r:chance(0)"), "true");
    EXPECT_EQ(lua("local r = m.random(1) return r:weightedIndex({0, 0, 1})"), "3");
    EXPECT_EQ(lua("local r = m.random(3) local t = r:shuffle({1, 2, 3, 4, 5}) table.sort(t) return table.concat(t)"), "12345");
    EXPECT_EQ(lua("local r = m.random(5) local first = r:nextFloat() r:reseed(5) return r:nextFloat() == first"), "true");
    EXPECT_EQ(lua("return type(m.random():nextFloat())"), "number");
    EXPECT_NE(lua("return m.random(1):weightedIndex({})").find("expected at least one weight"), std::string::npos);

    EXPECT_EQ(lua("local n = m.noise(4) return n:perlin(1.5, 2.5) == m.noise(4):perlin(1.5, 2.5)"), "true");
    EXPECT_EQ(lua("local n = m.noise(4) local v = n:simplex(0.3, 0.7) return v >= -1 and v <= 1"), "true");
    EXPECT_EQ(lua("local n = m.noise() return n:fractal(0.5, 0.5) ~= n:fractal(0.5, 0.5, 2, 3, 0.25)"), "true");
}

TEST_F(MathLuaTest, ProvidesScalarHelpers) {
    EXPECT_EQ(lua("return m.ease('linear', 0.25) .. ' ' .. m.ease('quadIn', 0.5)"), "0.25 0.25");
    EXPECT_NE(lua("return m.ease('bouncy', 0.5)").find("error: "), std::string::npos);
    EXPECT_EQ(lua("return m.clamp(5, 0, 1) .. ' ' .. m.lerp(0, 10, 0.5) .. ' ' .. m.inverseLerp(0, 10, 5)"), "1.0 5.0 0.5");
    EXPECT_EQ(lua("return m.remap(5, 0, 10, 100, 200) .. ' ' .. m.smoothstep(0, 1, 0.5) .. ' ' .. m.moveToward(0, 10, 3)"), "150.0 0.5 3.0");
    EXPECT_EQ(lua("return m.approximately(m.wrapAngle(3 * m.pi), m.pi) or m.approximately(m.wrapAngle(3 * m.pi), -m.pi)"), "true");
    EXPECT_EQ(lua("return m.dampFactor(10, 0) .. ' ' .. m.degrees(m.pi) .. ' ' .. tostring(m.approximately(m.radians(180), m.pi))"), "0.0 180.0 true");
    EXPECT_EQ(lua("return m.approximately(1, 1.05, 0.1) and not m.approximately(1, 1.05)"), "true");
    EXPECT_EQ(lua("return m.tau == 2 * m.pi and m.halfPi == m.pi / 2"), "true");
    EXPECT_EQ(lua("return m.sign(-3) .. ' ' .. m.sign(0) .. ' ' .. m.sign(0.2)"), "-1.0 0.0 1.0");
    EXPECT_EQ(lua("return m.saturate(1.5) .. ' ' .. m.saturate(-2) .. ' ' .. m.saturate(0.25)"), "1.0 0.0 0.25");
}

TEST_F(MathLuaTest, ConstructorsAcceptEveryValueForm) {
    EXPECT_EQ(lua("return tostring(m.vec2({3, 4})) .. ' ' .. tostring(m.vec2({x = 5, y = 6}))"), "Vec2(3.0, 4.0) Vec2(5.0, 6.0)");
    EXPECT_EQ(lua("local a = m.vec2(1, 2) local b = m.vec2(a) b.x = 9 return a.x .. ' ' .. b.x"), "1.0 9.0");
    EXPECT_EQ(lua("return tostring(m.rect({1, 2, 3, 4})) .. ' ' .. tostring(m.rect({x = 5, y = 6, width = 7, height = 8}))"), "Rect(1.0, 2.0, 3.0, 4.0) Rect(5.0, 6.0, 7.0, 8.0)");
    EXPECT_EQ(lua("local a = m.rect(1, 2, 3, 4) local b = m.rect(a) b.width = 0 return a.width .. ' ' .. b.width"), "3.0 0.0");
    EXPECT_EQ(lua("return m.color({1, 0.5, 0}):toHex() .. ' ' .. m.color({r = 0, g = 0, b = 1, a = 0.5}):toHex() .. ' ' .. m.color(m.black()):toHex()"), "#FFFF8000 #800000FF #FF000000");
    EXPECT_NE(lua("return m.vec2({1})").find("Expected a number in field 'y'."), std::string::npos);
    EXPECT_NE(lua("return m.rect(true)").find("Rect or table with x, y, width and height expected, got boolean"), std::string::npos);
    EXPECT_NE(lua("return m.color({1, 0})").find("Expected a number in field 'b'."), std::string::npos);
}

TEST_F(MathLuaTest, MeasuresCirclesAndSegments) {
    EXPECT_EQ(lua("return m.intersects({center = {0, 0}, radius = 5}, {{8, 0}, 3}) and not m.intersects({{0, 0}, 5}, {{9, 0}, 3})"), "true");
    EXPECT_EQ(lua("return m.intersects({{0, 0}, 5}, m.rect(4, -1, 10, 2)) and not m.intersects({{0, 0}, 5}, {4, 4, 10, 10})"), "true");
    EXPECT_EQ(lua("return tostring(m.intersection({{0, 0}, {10, 10}}, {start = {0, 10}, ['end'] = {10, 0}})) .. ' ' .. tostring(m.intersection({{0, 0}, {1, 0}}, {{0, 1}, {1, 1}}))"), "Vec2(5.0, 5.0) nil");
    EXPECT_EQ(lua("return tostring(m.closestPoint({{0, 0}, {10, 0}}, {4, 3})) .. ' ' .. m.distanceToSegment({{0, 0}, {10, 0}}, {13, 4})"), "Vec2(4.0, 0.0) 5.0");
    EXPECT_EQ(lua("return tostring(m.bounds({{3, 1}, {-2, 4}, {5, -1}})) .. ' ' .. tostring(m.bounds({}))"), "Rect(-2.0, -1.0, 7.0, 5.0) Rect(0.0, 0.0, 0.0, 0.0)");
    EXPECT_NE(lua("return m.intersects({radius = 5}, {{0, 0}, 1})").find("Expected a point in field 'center'."), std::string::npos);
    EXPECT_NE(lua("return m.closestPoint(5, {0, 0})").find("table with start and end expected"), std::string::npos);
}

TEST_F(MathLuaTest, AnalyzesPolygons) {
    lua("square = {{0, 0}, {10, 0}, {10, 10}, {0, 10}} dent = {{0, 0}, {10, 0}, {5, 3}, {10, 10}, {0, 10}}");
    EXPECT_EQ(lua("return m.polygonContains(square, {5, 5}) and not m.polygonContains(square, {15, 5})"), "true");
    EXPECT_EQ(lua("return m.polygonSignedArea(square) .. ' ' .. m.polygonCentroid(square).x"), "100.0 5.0");
    EXPECT_EQ(lua("return m.polygonConvex(square) and not m.polygonConvex(dent)"), "true");
    EXPECT_EQ(lua("return #m.convexHull({{0, 0}, {10, 0}, {5, 5}, {10, 10}, {0, 10}})"), "4");
    EXPECT_EQ(lua("local t = m.triangulate(dent) return #t .. ' ' .. math.min(table.unpack(t)) .. ' ' .. math.max(table.unpack(t))"), "9 1 5");
    EXPECT_NE(lua("return m.polygonSignedArea({{0, 0}, 'x'})").find("error: "), std::string::npos);
}

TEST_F(MathLuaTest, SamplesPoissonDisks) {
    EXPECT_EQ(lua("local points = m.poissonDisk({area = m.rect(0, 0, 200, 200), minimumDistance = 30, seed = 9}) return #points > 10"), "true");
    EXPECT_EQ(lua("local a = m.poissonDisk({area = {0, 0, 100, 100}, minimumDistance = 20, seed = 2}) local b = m.poissonDisk({area = {0, 0, 100, 100}, minimumDistance = 20, seed = 2}) return #a == #b and a[1] == b[1]"), "true");
    EXPECT_EQ(lua("local points = m.poissonDisk({area = {0, 0, 200, 200}, minimumDistance = 20, random = m.random(4), accept = function(p) return p.x < 100 end}) for _, p in ipairs(points) do if p.x >= 100 then return false end end return #points > 0"), "true");
    EXPECT_NE(lua("return m.poissonDisk({area = {0, 0, 100, 100}, minimumDistance = 20, accept = function() error('rejected') end})").find("rejected"), std::string::npos);
    EXPECT_EQ(lua("local options = {area = {0, 0, 200, 200}, minimumDistance = 20, random = m.random(4)} options.accept = function() options.random = nil collectgarbage() return true end local points = m.poissonDisk(options) local expected = m.poissonDisk({area = {0, 0, 200, 200}, minimumDistance = 20, random = m.random(4)}) return #points == #expected and points[#points] == expected[#expected]"), "true");
    EXPECT_NE(lua("return m.poissonDisk('area')").find("error: "), std::string::npos);
}

} // namespace haylen::math
