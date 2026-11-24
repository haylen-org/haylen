#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

class QuatLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("m = require('haylen.math')");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture;
};

TEST_F(QuatLuaTest, MakesRotations) {
    EXPECT_EQ(lua("return tostring(m.quat()) .. ' ' .. tostring(m.quat(0, 0, 0, 1) == m.quat({0, 0, 0, 1}))"), "Quat(0.0, 0.0, 0.0, 1.0) true");
    EXPECT_EQ(lua("local q = m.fromAxisAngle(m.up(), m.halfPi) return tostring(q:rotate(m.right()):approximately(m.forward())) .. ' ' .. tostring(m.approximately(q:angle(), m.halfPi)) .. ' ' .. tostring(q:axis():approximately(m.up()))"), "true true true");
    EXPECT_EQ(lua("local q = m.fromEuler({0.3, 0.5, 0.7}) local y, x, z = m.fromAxisAngle(m.up(), 0.5), m.fromAxisAngle(m.right(), 0.3), m.fromAxisAngle({0, 0, 1}, 0.7) return q:approximately(y * x * z)"), "true");
    EXPECT_EQ(lua("local q = m.fromEuler({0.3, 0.5, 0.7}, 'zyx') return q:toEuler('zyx'):approximately({0.3, 0.5, 0.7}, 0.0001) and q:toEuler():approximately({0.3, 0.5, 0.7}) == false"), "true");
    EXPECT_EQ(lua("return m.rotationBetween(m.right(), {0, 0, -3}):rotate(m.right()):approximately(m.forward())"), "true");
    EXPECT_EQ(lua("return m.lookRotation({0, 0, 1}):rotate(m.forward()):approximately({0, 0, 1})"), "true");
    EXPECT_EQ(lua("local q = m.fromAxisAngle({1, 2, 3}, 0.8) return m.quat(q:matrix():rotation()):approximately(q) and (q * q:inverse()):approximately(m.quat())"), "true");
    EXPECT_EQ(lua("local x, y, z, w = m.quat(0.5, 0.25, 0.125, 1):unpack() return x + y + z + w"), "1.875");
}

TEST_F(QuatLuaTest, InterpolatesAndSplitsRotations) {
    EXPECT_EQ(lua("local a, b = m.quat(), m.fromAxisAngle(m.up(), m.halfPi) return a:slerp(b, 0.5):approximately(m.fromAxisAngle(m.up(), m.pi / 4)) and a:nlerp(b, 1):approximately(b) and m.approximately(a:angleTo(b), m.halfPi)"), "true");
    EXPECT_EQ(lua("local b = m.fromAxisAngle(m.up(), m.halfPi) local opposite = m.quat(-b.x, -b.y, -b.z, -b.w) return m.quat():slerp(opposite, 1) == b and m.approximately(m.quat():dot(b), b.w)"), "true");
    EXPECT_EQ(lua("local twist, swing = m.fromAxisAngle(m.up(), 0.8), m.fromAxisAngle(m.right(), 0.5) local parts = (swing * twist):swingTwist(m.up()) return parts.twist:approximately(twist) and parts.swing:approximately(swing)"), "true");
    EXPECT_EQ(lua("local q = m.quat(0, 0, 0, 2) q.w = 4 return q:length() .. ' ' .. tostring(q:normalized())"), "4.0 Quat(0.0, 0.0, 0.0, 1.0)");
}

TEST_F(QuatLuaTest, RaisesClearErrors) {
    EXPECT_NE(lua("return m.fromAxisAngle({0, 0, 0}, 1)").find("The vector has no direction, because its length is zero."), std::string::npos);
    EXPECT_NE(lua("return m.quat():axis()").find("The rotation does not turn, so it has no axis."), std::string::npos);
    EXPECT_NE(lua("return m.quat(0, 0, 0, 0):inverse()").find("The quaternion has no inverse, because its length is zero or not a finite number."), std::string::npos);
    EXPECT_NE(lua("return m.lookRotation(m.up())").find("The up direction needs a finite length above zero and must not run along the direction to look along, or it cannot choose how the view turns around that direction."), std::string::npos);
    EXPECT_NE(lua("return m.fromEuler({0, 0, 0}, 'XYZ')").find("XYZ"), std::string::npos);
    EXPECT_NE(lua("return m.quat(1, 2)").find("number expected"), std::string::npos);
}

} // namespace haylen::lua::math
