#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

class Geometry3DLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("m = require('haylen.math')");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture;
};

TEST_F(Geometry3DLuaTest, CastsRaysAgainstEveryShape) {
    EXPECT_EQ(lua("local ray = m.ray3d({0, 10, 0}, {0, -2, 0}) local hit = ray:cast(m.triangle({-1, 0, 1}, {1, 0, 1}, {0, 0, -1})) return hit.distance .. ' ' .. tostring(hit.point) .. ' ' .. tostring(hit.normal)"), "10.0 Vec3(0.0, 0.0, 0.0) Vec3(0.0, 1.0, 0.0)");
    EXPECT_EQ(lua("local ray = m.ray3dBetween({-5, 0, 0}, {5, 0, 0}) return ray:cast(m.box({-1, -1, -1}, {1, 1, 1})).distance .. ' ' .. tostring(ray:cast(m.box({-1, -1, -1}, {1, 1, 1})).normal)"), "4.0 Vec3(-1.0, 0.0, 0.0)");
    EXPECT_EQ(lua("local ray = m.ray3d({0, 0, 10}, m.forward()) return ray:cast(m.sphere({0, 0, 0}, 2)).distance .. ' ' .. ray:cast(m.plane({0, 0, 1}, -3)).distance .. ' ' .. ray:cast(m.capsule({0, -1, 0}, {0, 1, 0}, 0.5)).distance"), "8.0 13.0 9.5");
    EXPECT_EQ(lua("local ray = m.ray3d({0, 0, 10}, m.forward()) return m.approximately(ray:cast(m.obb({0, 0, 0}, m.fromAxisAngle(m.up(), m.pi / 4), {1, 1, 1})).distance, 10 - math.sqrt(2), 0.0001)"), "true");
    EXPECT_EQ(lua("local ray = m.ray3d({0, 0, 10}, m.forward(), 5) return tostring(ray:cast(m.sphere({0, 0, 0}, 2))) .. ' ' .. tostring(ray:endPoint()) .. ' ' .. tostring(ray:at(2))"), "nil Vec3(0.0, 0.0, 5.0) Vec3(0.0, 0.0, 8.0)");
    EXPECT_EQ(lua("local hit = m.ray3d({0, 0, 0}, m.right()):cast(m.box({-1, -1, -1}, {1, 1, 1})) return hit.distance .. ' ' .. tostring(hit.normal:isZero())"), "0.0 true");
    EXPECT_NE(lua("return m.ray3d({0, 0, 0}, m.right()):cast(m.vec3())").find("A ray casts against a Triangle, a Box, an Obb, a Sphere, a Plane or a Capsule, not a userdata."), std::string::npos);
    EXPECT_NE(lua("local ray = m.ray3d({0, 0, 0}, m.right()) ray.direction = {2, 0, 0}").find("The direction of a ray must have a length of one. Make the ray with \"ray3d\" of \"haylen.math\", which scales its direction."), std::string::npos);
    EXPECT_NE(lua("return m.ray3d({0, 0, 0}, {0, 0, 0})").find("The vector has no direction, because its length is zero."), std::string::npos);
}

TEST_F(Geometry3DLuaTest, MeasuresAndCombinesShapes) {
    EXPECT_EQ(lua("local box = m.boxFromCenter({1, 2, 3}, {2, 4, 6}) return tostring(box.min) .. ' ' .. tostring(box:center()) .. ' ' .. tostring(box:halfExtents())"), "Vec3(0.0, 0.0, 0.0) Vec3(1.0, 2.0, 3.0) Vec3(1.0, 2.0, 3.0)");
    EXPECT_EQ(lua("local box = m.box({0, 0, 0}, {2, 2, 2}) return tostring(box:contains({1, 1, 1})) .. ' ' .. tostring(box:contains(m.box({0, 0, 0}, {3, 1, 1}))) .. ' ' .. tostring(box:intersects(m.box({2, 2, 2}, {3, 3, 3}))) .. ' ' .. tostring(box:intersects(m.sphere({4, 1, 1}, 1)))"), "true false true false");
    EXPECT_EQ(lua("local box = m.box({0, 0, 0}, {1, 1, 1}):merged({-1, 2, 0}) return tostring(box.min) .. ' ' .. tostring(box.max) .. ' ' .. tostring(box:merged(m.box({0, 0, 0}, {5, 5, 5})).max) .. ' ' .. tostring(box:expanded(1):size())"), "Vec3(-1.0, 0.0, 0.0) Vec3(1.0, 2.0, 1.0) Vec3(5.0, 5.0, 5.0) Vec3(4.0, 4.0, 3.0)");
    EXPECT_EQ(lua("local box = m.box({0, 0, 0}, {2, 1, 1}):transformed(m.compose({10, 0, 0}, m.fromAxisAngle(m.up(), m.halfPi))) return box.min:approximately({10, 0, -2}) and box.max:approximately({11, 1, 0}) and m.boxBounds({{1, 2, 3}, {-1, 5, 0}}) == m.box({-1, 2, 0}, {1, 5, 3})"), "true");
    EXPECT_EQ(lua("local s = m.sphere({0, 0, 0}, 1) return tostring(s:contains({0.5, 0, 0})) .. ' ' .. tostring(s:intersects(m.sphere({1.5, 0, 0}, 0.6))) .. ' ' .. tostring(s:bounds().max)"), "true true Vec3(1.0, 1.0, 1.0)");
    EXPECT_EQ(lua("local a = m.obb({0, 0, 0}, nil, {1, 1, 1}) local b = m.obb({2.3, 0, 0}, m.fromAxisAngle(m.up(), m.pi / 4), {1, 1, 1}) return tostring(a:intersects(b)) .. ' ' .. tostring(a:closestPoint({5, 0.5, -7})) .. ' ' .. tostring(a:contains({0.5, 0.5, 0.5}))"), "true Vec3(1.0, 0.5, -1.0) true");
    EXPECT_EQ(lua("local o = m.obbFromTransform(m.box({-1, -1, -1}, {1, 1, 1}), m.transform3d({5, 0, 0}, nil, {-2, 1, 1})) return tostring(o.halfExtents) .. ' ' .. tostring(o:bounds().min)"), "Vec3(2.0, 1.0, 1.0) Vec3(3.0, -1.0, -1.0)");
    EXPECT_EQ(lua("local c = m.capsule({0, 0, 0}, {0, 2, 0}, 0.5) return tostring(c:contains({0.4, 1, 0})) .. ' ' .. tostring(c:axis():length()) .. ' ' .. tostring(c:bounds().max)"), "true 2.0 Vec3(0.5, 2.5, 0.5)");
    EXPECT_EQ(lua("local t = m.triangle({0, 0, 0}, {4, 0, 0}, {0, 0, -4}) return t:area() .. ' ' .. tostring(t:normal()) .. ' ' .. tostring(t:closestPoint({1, 5, -1}))"), "8.0 Vec3(0.0, 1.0, 0.0) Vec3(1.0, 0.0, -1.0)");
    EXPECT_EQ(lua("local a, b = m.segment3d({0, 0, 0}, {10, 0, 0}):closestPoints(m.segment3d({5, 3, -1}, {5, 3, 1})) return tostring(a) .. ' ' .. tostring(b) .. ' ' .. tostring(m.segment3d({0, 0, 0}, {10, 0, 0}):closestPoint({-4, 1, 0}))"), "Vec3(5.0, 0.0, 0.0) Vec3(5.0, 3.0, 0.0) Vec3(0.0, 0.0, 0.0)");
    EXPECT_NE(lua("return m.sphere({0, 0, 0}, -1)").find("A radius must be a finite number of 0 or more."), std::string::npos);
    EXPECT_NE(lua("return m.triangle({0, 0, 0}, {1, 1, 1}, {2, 2, 2}):normal()").find("The triangle has no area, so it has no normal."), std::string::npos);
    EXPECT_NE(lua("return m.boxBounds({})").find("The bounds of points need at least one point."), std::string::npos);
    EXPECT_NE(lua("return m.box({0, 0, 0}, {1, 1, 1}):transformed(m.perspective())").find("A box is placed only by an affine matrix, whose last row is 0, 0, 0 and 1."), std::string::npos);
}

TEST_F(Geometry3DLuaTest, CullsWithTheFrustumOfACamera) {
    fixture.runLua("frustum = m.frustum(m.perspective({nearPlane = 0.5, farPlane = 100}) * m.lookAt({0, 0, 10}, {0, 0, 0}))");
    EXPECT_EQ(lua("return #frustum:planes() .. ' ' .. tostring(frustum:contains({0, 0, 0})) .. ' ' .. tostring(frustum:contains({0, 0, 20}))"), "6 true false");
    EXPECT_EQ(lua("return frustum:classify(m.sphere({0, 0, 0}, 1)) .. ' ' .. frustum:classify(m.sphere({0, 0, 9.5}, 1)) .. ' ' .. frustum:classify(m.boxFromCenter({0, 0, -200}, {1, 1, 1}))"), "inside intersecting outside");
    EXPECT_EQ(lua("return #m.frustum(m.perspective({nearPlane = 0.5}) * m.lookAt({0, 0, 10}, {0, 0, 0})):planes()"), "5");
    EXPECT_EQ(lua("local f = m.frustum(m.perspective({nearPlane = 1, farPlane = 10, depthRange = 'negativeOneToOne', reversedDepth = false}), 'negativeOneToOne') return tostring(f:contains({0, 0, -5})) .. ' ' .. tostring(f:contains({0, 0, -0.5}))"), "true false");
}

} // namespace haylen::lua::math
