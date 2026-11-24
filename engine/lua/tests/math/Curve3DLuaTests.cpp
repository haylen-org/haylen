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
