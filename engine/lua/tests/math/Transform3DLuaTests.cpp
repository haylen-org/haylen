#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

class Transform3DLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("m = require('haylen.math')");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture;
};

TEST_F(Transform3DLuaTest, PlacesChildrenInTheirParents) {
    EXPECT_EQ(lua("return tostring(m.transform3d())"), "Transform3D((0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0), (1.0, 1.0, 1.0))");
    EXPECT_EQ(lua("local world = m.transform3d({1, 2, 3}) * m.transform3d({4, 0, 0}) return world.x .. ' ' .. tostring(world.position)"), "5.0 Vec3(5.0, 2.0, 3.0)");
    EXPECT_EQ(lua("local t = m.transform3d({0, 1, 0}, m.fromAxisAngle(m.up(), m.halfPi), {2, 2, 2}) return t:apply({1, 0, 0}):approximately({0, 1, -2}) and t:applyVector({1, 0, 0}):approximately({0, 0, -2}) and t:applyInverse(t:apply({3, 4, 5})):approximately({3, 4, 5})"), "true");
    EXPECT_EQ(lua("local t = m.transform3d({1, 2, 3}, m.fromAxisAngle({1, 1, 0}, 0.5), {2, 2, 2}) return (t * t:inverse()):approximately(m.transform3d()) and t:matrix():approximately(m.compose({1, 2, 3}, m.fromAxisAngle({1, 1, 0}, 0.5), {2, 2, 2}))"), "true");
    EXPECT_EQ(lua("local t = m.transform3d(m.compose({1, 2, 3}, nil, {-1, 2, 3})) return tostring(t.position) .. ' ' .. tostring(t.scale:approximately({-1, 2, 3}))"), "Vec3(1.0, 2.0, 3.0) true");
    EXPECT_EQ(lua("local t = m.transform3d({0, 0, 0}, nil, {4, 1, 1}) return m.approximately(t:applyNormal({1, -1, 0}):dot(t:applyVector({1, 1, 0})), 0)"), "true");
    EXPECT_EQ(lua("local a, b = m.transform3d(), m.transform3d({10, 0, 0}, nil, {3, 3, 3}) local c = a:lerp(b, 0.5) return tostring(c.position) .. ' ' .. tostring(c.scale)"), "Vec3(5.0, 0.0, 0.0) Vec3(2.0, 2.0, 2.0)");
    EXPECT_EQ(lua("local t = m.transform3d() t.x, t.y, t.z = 1, 2, 3 t.scale = {2, 2, 2} t.rotation = m.fromAxisAngle(m.up(), 1) return tostring(t.position) .. ' ' .. m.transform3d(t).scale.x"), "Vec3(1.0, 2.0, 3.0) 2.0");
}

TEST_F(Transform3DLuaTest, RaisesClearErrors) {
    EXPECT_NE(lua("return m.transform3d({0, 0, 0}, nil, {1, 2, 1}):inverse()").find("A transform whose scale differs between axes has no inverse of the same form. Invert its matrix instead."), std::string::npos);
    EXPECT_NE(lua("return m.transform3d({0, 0, 0}, nil, {1, 0, 1}):applyInverse({1, 1, 1})").find("The transform scales an axis to zero or to infinity, so nothing undoes it."), std::string::npos);
}

TEST_F(Transform3DLuaTest, BlendsDualQuaternions) {
    EXPECT_EQ(lua("local joint = m.dualQuat(m.fromAxisAngle(m.up(), m.halfPi), {1, 2, 3}) return joint:translation():approximately({1, 2, 3}) and joint:apply(m.right()):approximately({1, 2, 2}) and joint:matrix():approximately(m.compose({1, 2, 3}, m.fromAxisAngle(m.up(), m.halfPi)))"), "true");
    EXPECT_EQ(lua("local lift = m.dualQuat(nil, {0, 5, 0}) local joint = m.dualQuat(m.fromAxisAngle(m.up(), 1), {1, 0, 0}) return (lift * joint):apply({1, 0, 0}):approximately(joint:apply({1, 0, 0}) + {0, 5, 0})"), "true");
    EXPECT_EQ(lua("local twisted = m.dualQuat(m.fromAxisAngle(m.right(), m.pi * 0.9)) local blended = m.blendDualQuats({m.dualQuat(), twisted}, {0.5, 0.5}) return m.approximately(blended:apply({0, 1, 0}):length(), 1) and m.approximately(blended:rotation():angle(), m.pi * 0.45)"), "true");
    EXPECT_NE(lua("return m.blendDualQuats({m.dualQuat()}, {1, 2})").find("A blend of dual quaternions needs one weight for each dual quaternion."), std::string::npos);
    EXPECT_NE(lua("return m.blendDualQuats({m.dualQuat()}, {0})").find("The weights of a blend of dual quaternions add up to zero, so they blend nothing."), std::string::npos);
}

} // namespace haylen::lua::math
