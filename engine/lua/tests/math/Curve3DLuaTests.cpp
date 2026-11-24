#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

class Curve3DLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("m = require('haylen.math')");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture;
};

TEST_F(Curve3DLuaTest, MakesSplinesOfEveryKind) {
    EXPECT_EQ(lua("local s = m.spline3d({{0, 0, 0}, {3, 0, 0}, {3, 4, 0}}, {kind = 'linear'}) return s.kind .. ' ' .. s.degree .. ' ' .. s.segmentCount .. ' ' .. string.format('%.2f', s.length)"), "linear 1 2 7.00");
    EXPECT_EQ(lua("local s = m.spline3d({{0, 0, 0}, {1, 2, 0}, {2, 0, 0}}, {kind = 'quadraticBezier'}) return tostring(s:point(0.5))"), "Vec3(1.0, 1.0, 0.0)");
    EXPECT_EQ(lua("local s = m.spline3d({{0, 0, 0}, {1, 4, 0}, {6, 4, 1}, {7, 0, 0}}, {alpha = 1, tension = 0.2}) return tostring(s:point(1 / 3):approximately({1, 4, 0}, 1e-4))"), "true");
    EXPECT_EQ(lua("local s = m.spline3d({{1, 0, 0}, {1, 1, 0}, {0, 1, 0}}, {kind = 'nurbs', degree = 2, weights = {1, math.sqrt(0.5), 1}}) return string.format('%.4f %d %d', s:point(0.37):length(), #s.knots, #s.weights)"), "1.0000 6 3");
    EXPECT_EQ(lua("local s = m.spline3d({{0, 0, 0}, {0, 0, -10}}) local d = s:derivatives(0.5) return tostring(d.point) .. ' ' .. tostring(d.first) .. ' ' .. tostring(s.closed) .. ' ' .. #s.points"), "Vec3(0.0, 0.0, -5.0) Vec3(0.0, 0.0, -10.0) false 2");
    EXPECT_NE(lua("return m.spline3d({{0, 0, 0}, {1, 0, 0}}, {kind = 'bezier'})").find("A Bezier spline needs 3n + 1 points when open and 3n points when closed."), std::string::npos);
    EXPECT_NE(lua("return m.spline3d({{0, 0, 0}, {1, 0, 0}}, {weights = {1, 1}})").find("Only a NURBS spline takes weights, knots and a degree."), std::string::npos);
    EXPECT_NE(lua("return m.spline3d({{0, 0, 0}, {1, 0, 0}}, {kind = 'nurbs', degree = 4})").find("The NURBS spline needs at least one point more than its degree."), std::string::npos);
    EXPECT_NE(lua("return m.spline3d({{0, 0, 0}, {1, 0, 0}}, {bend = 1})").find("Unknown option \"bend\"."), std::string::npos);
    EXPECT_NE(lua("return m.spline3d({{0, 0, 0}, {0, 0, 0}}):tangent(0.5)").find("The spline stands still at this parameter, so it has no direction."), std::string::npos);
}

TEST_F(Curve3DLuaTest, MeasuresSamplesAndFramesSplines) {
    fixture.runLua("rail = m.spline3d({{0, 0, 0}, {10, 0, 0}, {10, 0, -10}}, {kind = 'linear'})");
    EXPECT_EQ(lua("return tostring(rail:pointAtDistance(15)) .. ' ' .. tostring(rail:tangentAtDistance(15)) .. ' ' .. rail:parameterAtDistance(10)"), "Vec3(10.0, 0.0, -5.0) Vec3(0.0, 0.0, -1.0) 0.5");
    EXPECT_EQ(lua("local c = rail:closestPoint({4, 2, 0}) return string.format('%.3f %.1f %.1f', c.t, c.distance, c.point.x)"), "0.200 2.0 4.0");
    EXPECT_EQ(lua("return #rail:sample(3) .. ' ' .. #rail:sampleEvenly(5) .. ' ' .. #rail:sampleByDistance(4)"), "3 5 6");
    EXPECT_EQ(lua("local frames = rail:frames(3) return #frames .. ' ' .. tostring(frames[1].normal) .. ' ' .. tostring(frames[1].binormal) .. ' ' .. tostring(frames[3].tangent)"), "3 Vec3(0.0, 1.0, 0.0) Vec3(0.0, 0.0, 1.0) Vec3(0.0, 0.0, -1.0)");
    EXPECT_EQ(lua("return tostring(rail:frames(2, {up = {0, 0, -1}})[1].normal)"), "Vec3(0.0, 0.0, -1.0)");
    EXPECT_NE(lua("return rail:frames(1)").find("The frames of a curve need at least 2 frames, one at each end."), std::string::npos);
    EXPECT_NE(lua("return rail:frames(4, {up = {0, 0, 0}})").find("The vector has no direction, because its length is zero."), std::string::npos);
}

TEST_F(Curve3DLuaTest, JoinsCurvesIntoPaths) {
    fixture.runLua("first = m.spline3d({{0, 0, 0}, {10, 0, 0}}, {kind = 'linear'}) second = m.spline3d({{10, 0, 0}, {10, 0, -5}}, {kind = 'linear'}) road = m.curvePath3d({first, second})");
    EXPECT_EQ(lua("return road.curveCount .. ' ' .. tostring(road.closed) .. ' ' .. string.format('%.1f', road.length) .. ' ' .. table.concat(road.joins, ',')"), "2 false 15.0 position");
    EXPECT_EQ(lua("return tostring(road:pointAtDistance(12)) .. ' ' .. tostring(road:tangentAtDistance(12)) .. ' ' .. tostring(road:point(1))"), "Vec3(10.0, 0.0, -2.0) Vec3(0.0, 0.0, -1.0) Vec3(10.0, 0.0, -5.0)");
    EXPECT_EQ(lua("local c = road:closestPoint({5, 1, 0}) return string.format('%.3f %.1f', c.t, c.distance) .. ' ' .. #road:sampleEvenly(4) .. ' ' .. #road:sampleByDistance(5) .. ' ' .. #road:frames(4)"), "0.333 1.0 4 4 4");
    EXPECT_NE(lua("return m.curvePath3d({first, first})").find("The curves of a path must join"), std::string::npos);
    EXPECT_NE(lua("return m.curvePath3d({})").find("A curve path needs at least one curve."), std::string::npos);
}

TEST_F(Curve3DLuaTest, DrawsEllipsesAndArcsAsSplines) {
    EXPECT_EQ(lua("local e = m.ellipse3d({0, 0, 0}, {2, 0, 0}, {0, 0, -1}) return e.kind .. ' ' .. tostring(e.closed) .. ' ' .. e.segmentCount .. ' ' .. tostring(e:point(0.25):approximately({0, 0, -1}, 1e-5))"), "nurbs true 4 true");
    EXPECT_EQ(lua("local e = m.ellipse3d({0, 0, 0}, {2, 0, 0}, {0, 1, 0}, {startAngle = 0, endAngle = math.pi}) return tostring(e.closed) .. ' ' .. tostring(e:point(1):approximately({-2, 0, 0}, 1e-5))"), "false true");
    EXPECT_EQ(lua("local a = m.arc3d({0, 0, 0}, {5, 0, 0}, m.up(), math.pi / 2) return string.format('%.2f', a.length) .. ' ' .. tostring(a:point(1):approximately({0, 0, -5}, 1e-5))"), "7.85 true");
    EXPECT_EQ(lua("local a = m.arcThrough3d({1, 0, 0}, {0, 0, -1}, {-1, 0, 0}) return string.format('%.2f %.3f', a.length, a:closestPoint({0, 0, -1}).distance)"), "3.14 0.000");
    EXPECT_NE(lua("return m.ellipse3d({0, 0, 0}, {1, 0, 0}, {2, 0, 0})").find("The center and the two axes of an ellipse must be finite, and the axes must run in different directions so they span a plane."), std::string::npos);
    EXPECT_NE(lua("return m.ellipse3d({0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {endAngle = 0})").find("The angles of an ellipse must be finite and apart by more than 0 and at most a whole turn."), std::string::npos);
    EXPECT_NE(lua("return m.ellipse3d({0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {turns = 1})").find("Unknown option \"turns\"."), std::string::npos);
    EXPECT_NE(lua("return m.arc3d({0, 0, 0}, {0, 2, 0}, m.up(), 1)").find("The start of an arc lies on its axis, so the arc has no radius."), std::string::npos);
    EXPECT_NE(lua("return m.arc3d({0, 0, 0}, {1, 0, 0}, m.up(), 0)").find("The angle of an arc must be a finite number of radians other than 0 and at most a whole turn."), std::string::npos);
    EXPECT_NE(lua("return m.arcThrough3d({0, 0, 0}, {1, 0, 0}, {2, 0, 0})").find("The three points of an arc must be finite and must not lie on one line, or no circle passes through them."), std::string::npos);
    EXPECT_NE(lua("return m.spline3d({{0, 0, 0}, {1, 0, 0}}, {kind = 'nurbs', degree = 1, closed = true})").find("A closed NURBS spline must end where it starts."), std::string::npos);
}

TEST_F(Curve3DLuaTest, WindsParametricCurvesAndSamplesThemInBatches) {
    fixture.runLua("spring = m.helix({radius = 2, pitch = 0.5, turns = 3})");
    EXPECT_EQ(lua("return tostring(spring.closed) .. ' ' .. string.format('%.1f', spring.length) .. ' ' .. tostring(spring:point(1 / 12):approximately({0, 0.125, -2}, 1e-5))"), "false 37.7 true");
    EXPECT_EQ(lua("local points = spring:sample(13) return #points .. ' ' .. tostring(points[13]:approximately({2, 1.5, 0}, 1e-4)) .. ' ' .. #spring:sampleEvenly(4) .. ' ' .. #spring:sampleByDistance(10)"), "13 true 4 5");
    EXPECT_EQ(lua("local d = spring:derivatives(0) return tostring(d.point:approximately({2, 0, 0})) .. ' ' .. string.format('%.3f', d.first.y) .. ' ' .. tostring(spring:tangentAtDistance(0):approximately(spring:tangent(0), 1e-6))"), "true 1.500 true");
    EXPECT_EQ(lua("local c = spring:closestPoint({0, 0.125, -3}) return string.format('%.3f %.3f %.3f', c.t, c.distance, spring:parameterAtDistance(spring.length))"), "0.083 1.000 1.000");
    EXPECT_EQ(lua("local s = m.helix({radius = 1, endRadius = 3, pitch = 0, turns = -2}) return tostring(s:pointAtDistance(s.length):approximately({3, 0, 0}, 1e-4))"), "true");
    EXPECT_EQ(lua("local knot = m.torusKnot({p = 3, q = 2}) local frames = knot:frames(16) return tostring(knot.closed) .. ' ' .. #frames .. ' ' .. tostring(frames[16].position:approximately(frames[1].position, 1e-4))"), "true 16 true");
    EXPECT_EQ(lua("local figure = m.lissajous3d({amplitude = {2, 1, 3}, phase = {math.pi / 2, 0, 0}}) return tostring(figure.closed) .. ' ' .. tostring(figure:point(0.25):approximately({0, 0, -3}, 1e-5))"), "true true");
    EXPECT_NE(lua("return m.helix({turns = 0})").find("A helix needs a finite number of turns other than 0 and at most 16384 either way."), std::string::npos);
    EXPECT_NE(lua("return m.helix({radius = -1})").find("The radius and the end radius of a helix must be finite numbers of 0 or more."), std::string::npos);
    EXPECT_NE(lua("return m.torusKnot({p = 2, q = 4})").find("The windings p and q of a torus knot must be whole numbers from 1 to 1024 with no common divisor above 1, or the knot runs over itself."), std::string::npos);
    EXPECT_NE(lua("return m.torusKnot({tubeRadius = 2})").find("A torus knot needs a finite tube radius above 0 and below its radius."), std::string::npos);
    EXPECT_NE(lua("return m.lissajous3d({frequency = {1, 2.5, 3}})").find("The frequencies of a Lissajous curve must be whole numbers from 1 to 1024."), std::string::npos);
    EXPECT_NE(lua("return m.helix({width = 1})").find("Unknown option \"width\"."), std::string::npos);
    EXPECT_NE(lua("return m.helix({radius = 0, pitch = 0}):tangent(0.5)").find("The curve stands still at this parameter, so it has no direction."), std::string::npos);
}

TEST_F(Curve3DLuaTest, EvaluatesSurfacesAndVolumes) {
    fixture.runLua("sheet = m.nurbsSurface({{0, 0, 0}, {2, 0, 0}, {0, 0, -2}, {2, 0, -2}}, {columns = 2, degreeU = 1, degreeV = 1})");
    EXPECT_EQ(lua("return sheet.columns .. ' ' .. sheet.rows .. ' ' .. tostring(sheet:point(0.5, 0.25)) .. ' ' .. tostring(sheet:normal(0.5, 0.5))"), "2 2 Vec3(1.0, 0.0, -0.5) Vec3(0.0, 1.0, 0.0)");
    EXPECT_EQ(lua("local d = sheet:derivatives(0.5, 0.5) return tostring(d.alongU) .. ' ' .. tostring(d.alongV)"), "Vec3(2.0, 0.0, 0.0) Vec3(0.0, 0.0, -2.0)");
    EXPECT_NE(lua("return m.nurbsSurface({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}, {columns = 2})").find("A NURBS surface needs its points in whole rows of a number of columns above 0."), std::string::npos);
    fixture.runLua("block = m.nurbsVolume({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}, {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}}, {columns = 2, rows = 2, degreeU = 1, degreeV = 1, degreeW = 1})");
    EXPECT_EQ(lua("return block.columns .. ' ' .. block.rows .. ' ' .. block.layers .. ' ' .. tostring(block:point(0.25, 0.5, 0.75))"), "2 2 2 Vec3(0.25, 0.5, 0.75)");
    EXPECT_NE(lua("return m.nurbsVolume({{0, 0, 0}}, {columns = 1, rows = 1})").find("The NURBS volume across its columns needs at least one point more than its degree."), std::string::npos);
}

} // namespace haylen::lua::math
