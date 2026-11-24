#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

class VectorLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("m = require('haylen.math')");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture;
};

TEST_F(VectorLuaTest, MakesVec3FromNumbersTablesAndCopies) {
    EXPECT_EQ(lua("return tostring(m.vec3())"), "Vec3(0.0, 0.0, 0.0)");
    EXPECT_EQ(lua("return tostring(m.vec3(1, 2, 3))"), "Vec3(1.0, 2.0, 3.0)");
    EXPECT_EQ(lua("return tostring(m.vec3({x = 4, y = 5, z = 6}) + {1, 1, 1})"), "Vec3(5.0, 6.0, 7.0)");
    EXPECT_EQ(lua("local a = m.vec3(1, 2, 3) local b = m.vec3(a) b.z = 9 return a.z .. ' ' .. b.z"), "3.0 9.0");
    EXPECT_EQ(lua("return m.vec3(1, 2, 3):unpack()"), "1.0");
    EXPECT_EQ(lua("local x, y, z = m.vec3(1, 2, 3):unpack() return x + y + z"), "6.0");
    EXPECT_NE(lua("return m.vec3({x = 1, y = 2})").find("Expected a number in field \"z\"."), std::string::npos);
    EXPECT_NE(lua("return m.vec3(1, 2, 3):dot(5)").find("Vec3 or table with x, y and z expected, got number"), std::string::npos);
    EXPECT_NE(lua("return m.vec3().w").find("The type \"haylen.Vec3\" has no member \"w\"."), std::string::npos);
}

TEST_F(VectorLuaTest, AppliesOperatorsThatReturnNewValues) {
    EXPECT_EQ(lua("return tostring(m.vec3(1, 2, 3) - {1, 1, 1})"), "Vec3(0.0, 1.0, 2.0)");
    EXPECT_EQ(lua("return tostring(2 * m.vec3(1, 2, 3) * m.vec3(1, 0.5, 2))"), "Vec3(2.0, 2.0, 12.0)");
    EXPECT_EQ(lua("return tostring(m.vec3(2, 4, 8) / 2) .. ' ' .. tostring(m.vec3(2, 4, 8) / {2, 4, 8})"), "Vec3(1.0, 2.0, 4.0) Vec3(1.0, 1.0, 1.0)");
    EXPECT_EQ(lua("return tostring(-m.vec3(1, -2, 3))"), "Vec3(-1.0, 2.0, -3.0)");
    EXPECT_EQ(lua("return m.vec3(1, 2, 3) == m.vec3(1, 2, 3) and m.vec3(1, 2, 3) ~= m.vec3(3, 2, 1)"), "true");
    EXPECT_EQ(lua("local a = m.vec3(1, 2, 3) local b = a + {0, 0, 0} b.x = 7 return a.x"), "1.0");
}

TEST_F(VectorLuaTest, MeasuresAndCombinesVec3) {
    EXPECT_EQ(lua("local v = m.vec3(2, 3, 6) return v:length() .. ' ' .. v:lengthSquared()"), "7.0 49.0");
    EXPECT_EQ(lua("return tostring(m.right():cross(m.up())) .. ' ' .. tostring(m.forward())"), "Vec3(0.0, 0.0, 1.0) Vec3(0.0, 0.0, -1.0)");
    EXPECT_EQ(lua("return m.vec3(1, 2, 3):dot({4, 5, 6}) .. ' ' .. m.vec3():distance({2, 3, 6}) .. ' ' .. m.vec3():distanceSquared({1, 1, 1})"), "32.0 7.0 3.0");
    EXPECT_EQ(lua("return tostring(m.vec3(0, 0, 5):normalized())"), "Vec3(0.0, 0.0, 1.0)");
    EXPECT_EQ(lua("return tostring(m.vec3(3, 4, 5):project({0, 2, 0})) .. ' ' .. tostring(m.vec3(3, 4, 5):projectOnPlane(m.up()))"), "Vec3(0.0, 4.0, 0.0) Vec3(3.0, 0.0, 5.0)");
    EXPECT_EQ(lua("return tostring(m.vec3(1, -1, 0):reflect(m.up()))"), "Vec3(1.0, 1.0, 0.0)");
    EXPECT_EQ(lua("local d = m.vec3(1, -1, 0):normalized() return m.approximately(d:refract(m.up(), 1):dot(d), 1) and d:refract(m.up(), 1.5) == nil"), "true");
    EXPECT_EQ(lua("return tostring(m.vec3(0, 0, 0):lerp({10, 20, 30}, 0.5)) .. ' ' .. tostring(m.vec3(1, 5, 0):min({2, 3, -1})) .. ' ' .. tostring(m.vec3(1, 5, 0):max({2, 3, -1}))"), "Vec3(5.0, 10.0, 15.0) Vec3(1.0, 3.0, -1.0) Vec3(2.0, 5.0, 0.0)");
    EXPECT_EQ(lua("return tostring(m.vec3(1, 2, 3):approximately({1.000001, 2, 3})) .. ' ' .. tostring(m.vec3(1, 2, 3):approximately({1.1, 2, 3}, 0.2)) .. ' ' .. tostring(m.vec3(1, 2, 3):approximately({1.1, 2, 3}))"), "true true false");
    EXPECT_EQ(lua("return tostring(m.vec3():isZero()) .. ' ' .. tostring(m.vec3(1 / 0, 0, 0):isFinite())"), "true false");
}

TEST_F(VectorLuaTest, RaisesClearErrorsForVectorsWithoutDirection) {
    EXPECT_NE(lua("return m.vec3():normalized()").find("The vector has no direction, because its length is zero."), std::string::npos);
    EXPECT_NE(lua("return m.vec3(0 / 0, 1, 0):normalized()").find("The vector has no direction, because one of its components is not a finite number."), std::string::npos);
    EXPECT_NE(lua("return m.vec3(1, 2, 3):project({0, 0, 0})").find("A vector projects only onto a vector with a direction, and this one has a zero or infinite length."), std::string::npos);
    EXPECT_NE(lua("return m.vec4():normalized()").find("The vector has no direction, because its length is zero."), std::string::npos);
}

TEST_F(VectorLuaTest, WorksWithVec4) {
    EXPECT_EQ(lua("return tostring(m.vec4()) .. ' ' .. tostring(m.vec4(1, 2, 3, 4))"), "Vec4(0.0, 0.0, 0.0, 0.0) Vec4(1.0, 2.0, 3.0, 4.0)");
    EXPECT_EQ(lua("return tostring(m.vec4({1, 2, 3, 4})) .. ' ' .. tostring(m.vec4(m.vec3(1, 2, 3), 1)) .. ' ' .. tostring(m.vec4({1, 2, 3}, 0))"), "Vec4(1.0, 2.0, 3.0, 4.0) Vec4(1.0, 2.0, 3.0, 1.0) Vec4(1.0, 2.0, 3.0, 0.0)");
    EXPECT_EQ(lua("local v = m.vec4(1, 2, 3, 4) return tostring(v:xyz()) .. ' ' .. v.w .. ' ' .. v:dot({1, 1, 1, 1})"), "Vec3(1.0, 2.0, 3.0) 4.0 10.0");
    EXPECT_EQ(lua("return m.vec4(1, 1, 1, 1):length() .. ' ' .. tostring(m.vec4(0, 0, 0, 3):normalized())"), "2.0 Vec4(0.0, 0.0, 0.0, 1.0)");
    EXPECT_EQ(lua("return tostring((m.vec4(1, 2, 3, 4) + {1, 1, 1, 1}) * 2 - m.vec4(4, 6, 8, 10))"), "Vec4(0.0, 0.0, 0.0, 0.0)");
    EXPECT_EQ(lua("return tostring(m.vec4():lerp({2, 4, 6, 8}, 0.5)) .. ' ' .. tostring(m.vec4(1, 2, 3, 4):approximately({1, 2, 3, 4.000001}))"), "Vec4(1.0, 2.0, 3.0, 4.0) true");
    EXPECT_NE(lua("return m.vec4({1, 2, 3})").find("Expected a number in field \"w\"."), std::string::npos);
}

} // namespace haylen::lua::math
