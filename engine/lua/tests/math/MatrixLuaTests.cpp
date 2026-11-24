#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

class MatrixLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("m = require('haylen.math')");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture;
};

TEST_F(MatrixLuaTest, MakesMatricesByColumns) {
    EXPECT_EQ(lua("return tostring(m.mat3())"), "Mat3((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))");
    EXPECT_EQ(lua("local a = m.mat3(1, 2, 3, 4, 5, 6, 7, 8, 10) return a:get(1, 2) .. ' ' .. tostring(a:column(3)) .. ' ' .. tostring(a:row(1))"), "4.0 Vec3(7.0, 8.0, 10.0) Vec3(1.0, 4.0, 7.0)");
    EXPECT_EQ(lua("local a = m.mat3({2, 0, 0, 0, 2, 0, 0, 0, 2}) return a:determinant() .. ' ' .. tostring(a * {1, 2, 3}) .. ' ' .. tostring((a * a):get(3, 3))"), "8.0 Vec3(2.0, 4.0, 6.0) 4.0");
    EXPECT_EQ(lua("local a = m.mat3(1, 2, 3, 4, 5, 6, 7, 8, 10) return (a * a:inverse()):approximately(m.mat3()) and a:transposed():get(1, 2) == 2"), "true");
    EXPECT_EQ(lua("return tostring(m.mat4():column(4)) .. ' ' .. select('#', m.mat4():unpack())"), "Vec4(0.0, 0.0, 0.0, 1.0) 16");
    EXPECT_EQ(lua("local a = m.mat4() local b = m.mat4(a) b = b * m.compose({1, 2, 3}) return a == m.mat4() and tostring(b:translation())"), "Vec3(1.0, 2.0, 3.0)");
    EXPECT_NE(lua("return m.mat3({1, 2, 3})").find("A matrix table lists its 9 numbers by columns, and this one holds 3 values."), std::string::npos);
    EXPECT_NE(lua("return m.mat4():get(5, 1)").find("A row or a column of this matrix is a whole number from 1 to 4, not 5."), std::string::npos);
}

TEST_F(MatrixLuaTest, PlacesPointsAndDirections) {
    EXPECT_EQ(lua("local t = m.compose({1, 2, 3}, m.fromAxisAngle(m.up(), m.halfPi), {2, 2, 2}) return tostring(t:apply({1, 0, 0}):approximately({1, 2, 1})) .. ' ' .. tostring(t:applyVector({1, 0, 0}):approximately({0, 0, -2}))"), "true true");
    EXPECT_EQ(lua("local t = m.compose({1, 2, 3}) return tostring(t * m.vec4(0, 0, 0, 1)) .. ' ' .. tostring(t * {0, 0, 0, 0})"), "Vec4(1.0, 2.0, 3.0, 1.0) Vec4(0.0, 0.0, 0.0, 0.0)");
    EXPECT_EQ(lua("local t = m.compose({5, 0, 0}, m.fromAxisAngle({1, 1, 0}, 0.4), {2, 3, 4}) return (t * t:inverse()):approximately(m.mat4(), 0.0001) and (t:affineInverse() * t):approximately(m.mat4(), 0.0001) and t:isAffine() and t:isInvertible()"), "true");
    EXPECT_EQ(lua("local parts = m.compose({4, 5, 6}, m.fromAxisAngle(m.up(), 0.5), {-1, 2, 3}):decompose() return tostring(parts.translation) .. ' ' .. tostring(parts.scale:approximately({-1, 2, 3})) .. ' ' .. tostring(parts.rotation:approximately(m.fromAxisAngle(m.up(), 0.5), 0.0001) or parts.rotation:angle() > 0)"), "Vec3(4.0, 5.0, 6.0) true true");
    EXPECT_EQ(lua("return tostring(m.compose({0, 0, 0}, nil, {2, 1, 1}):normalMatrix() * {1, 0, 0})"), "Vec3(0.5, 0.0, 0.0)");
    EXPECT_EQ(lua("return tostring(m.compose({0, 0, 0}, nil, {2, 1, 1}):linear():get(1, 1))"), "2.0");
    EXPECT_NE(lua("return m.compose({0, 0, 0}, nil, {1, 0, 1}):inverse()").find("The matrix has no inverse, because it squashes space flat and its determinant is zero."), std::string::npos);
    EXPECT_NE(lua("return m.compose({0, 0, 0}, nil, {1, 0, 1}):decompose()").find("The matrix scales an axis to zero or to infinity, so it holds no rotation."), std::string::npos);
}

TEST_F(MatrixLuaTest, BuildsCamerasAndProjections) {
    EXPECT_EQ(lua("local view = m.lookAt({0, 0, 10}, {0, 0, 0}) return tostring(view:apply({0, 0, 0}):approximately({0, 0, -10}))"), "true");
    EXPECT_EQ(lua("local p = m.perspective({fieldOfView = m.radians(60), aspect = 2, nearPlane = 0.5, farPlane = 50}) return m.approximately(p:apply({0, 0, -0.5}).z, 1) and m.approximately(p:apply({0, 0, -50}).z, 0)"), "true");
    EXPECT_EQ(lua("local p = m.perspective({nearPlane = 1, farPlane = 10, depthRange = 'negativeOneToOne', reversedDepth = false}) return m.approximately(p:apply({0, 0, -1}).z, -1) and m.approximately(p:apply({0, 0, -10}).z, 1)"), "true");
    EXPECT_EQ(lua("local p = m.perspective({nearPlane = 1, farPlane = 10, reversedDepth = false}) local q = m.perspective({nearPlane = 1, farPlane = 10, depthRange = 'negativeOneToOne', reversedDepth = false}) return (m.depthRangeConversion('zeroToOne', 'negativeOneToOne') * p):approximately(q)"), "true");
    EXPECT_EQ(lua("local p = m.asymmetricPerspective({left = -0.5, right = 0.5, bottom = -0.5, top = 0.5, nearPlane = 0.1}) return m.approximately(p:apply({math.tan(0.5), 0, -1}).x, 1)"), "true");
    EXPECT_EQ(lua("local p = m.orthographic({left = 0, right = 10, bottom = 0, top = 5, nearPlane = 1, farPlane = 3}) return tostring(p:apply({10, 5, -1}):approximately({1, 1, 1}))"), "true");
    EXPECT_EQ(lua("local p = m.perspective({nearPlane = 0.1, farPlane = 100}) local o = p:obliqueNearPlane(m.planeFromPoint({0, 0, -1}, {0, 0, -5})) return m.approximately(o:apply({0.2, 0.1, -5}).z, 1, 0.001) and o:apply({0, 0, -3}).z > 1"), "true");
    EXPECT_NE(lua("return m.perspective({fieldOfView = 4})").find("The field of view of a perspective must be an angle above 0 and below pi radians."), std::string::npos);
    EXPECT_NE(lua("return m.perspective({near = 1})").find("Unknown option \"near\"."), std::string::npos);
    EXPECT_NE(lua("return m.perspective({depthRange = 'minusOneToOne'})").find("depthRange"), std::string::npos);
    EXPECT_NE(lua("return m.lookAt({1, 1, 1}, {1, 1, 1})").find("The eye and the target of a view are the same point, so the view has no direction."), std::string::npos);
    EXPECT_NE(lua("return m.perspective():obliqueNearPlane(m.planeFromPoint({0, 0, 1}, {0, 0, -5}))").find("An oblique near plane must face away from the camera, so the camera must lie behind the plane."), std::string::npos);
}

TEST_F(MatrixLuaTest, WorksWithPlanes) {
    EXPECT_EQ(lua("local p = m.plane({0, 2, 0}, 4) return tostring(p) .. ' ' .. p:signedDistance({0, 5, 0}) .. ' ' .. tostring(p:closestPoint({1, 5, 1}))"), "Plane((0.0, 1.0, 0.0), 2.0) 3.0 Vec3(1.0, 2.0, 1.0)");
    EXPECT_EQ(lua("local p = m.planeFromPoints({0, 1, 0}, {0, 1, 1}, {1, 1, 0}) return tostring(p.normal) .. ' ' .. p.distance .. ' ' .. tostring(p:coefficients())"), "Vec3(0.0, 1.0, 0.0) 1.0 Vec4(0.0, 1.0, 0.0, -1.0)");
    EXPECT_EQ(lua("local p = m.plane(m.up(), 0) p.distance = 3 p.normal = m.right() return tostring(p)"), "Plane((1.0, 0.0, 0.0), 3.0)");
    EXPECT_NE(lua("local p = m.plane(m.up(), 0) p.normal = {0, 2, 0}").find("The normal of a plane must have a length of one. Make the plane with \"plane\" of \"haylen.math\", which scales its normal."), std::string::npos);
    EXPECT_NE(lua("return m.planeFromPoints({0, 0, 0}, {1, 1, 1}, {2, 2, 2})").find("The three points lie on one line, so they span no plane."), std::string::npos);
    EXPECT_NE(lua("return m.plane({0, 0, 0}, 1)").find("The vector has no direction, because its length is zero."), std::string::npos);
}

TEST_F(MatrixLuaTest, MultipliesAndAppliesListsOfMatrices) {
    fixture.runLua("parent = m.compose({0, 10, 0}, m.fromAxisAngle(m.up(), 0.5)) children = {m.compose({1, 0, 0}), m.compose({0, 0, -3}, m.fromAxisAngle(m.right(), 0.2), {2, 2, 2})}");
    EXPECT_EQ(lua("local worlds = m.matrixBatch.multiply(parent, children) return #worlds .. ' ' .. tostring(worlds[1]:approximately(parent * children[1])) .. ' ' .. tostring(worlds[2]:approximately(parent * children[2]))"), "2 true true");
    EXPECT_EQ(lua("local pairs = m.matrixBatch.multiplyPairs(children, {parent, parent}) return tostring(pairs[2]:approximately(children[2] * parent))"), "true");
    EXPECT_EQ(lua("local points = m.matrixBatch.transformPoints(parent, {{0, 0, 0}, {1, 0, 0}}) return tostring(points[1]) .. ' ' .. tostring(points[2]:approximately(parent:apply({1, 0, 0})))"), "Vec3(0.0, 10.0, 0.0) true");
    EXPECT_EQ(lua("return #m.matrixBatch.multiply(parent, {}) .. ' ' .. type(m.matrixBatch.instructions)"), "0 string");
    EXPECT_NE(lua("return m.matrixBatch.multiplyPairs(children, {parent})").find("The two lists of matrices must hold as many matrices each, but they hold 2 and 1."), std::string::npos);
}

} // namespace haylen::lua::math
