#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

class SphericalLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("m = require('haylen.math')");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture;
};

TEST_F(SphericalLuaTest, ConvertsSphericalAndCylindricalCoordinates) {
    EXPECT_EQ(lua("local s = m.spherical(2, math.pi / 2, math.pi / 2) return tostring(s:vector():approximately({2, 0, 0}, 1e-6)) .. ' ' .. s.radius"), "true 2.0");
    EXPECT_EQ(lua("local s = m.spherical({0, 0, 3}) return tostring(s)"), "Spherical(3.0, 1.5707963705062866, 0.0)");
    EXPECT_EQ(lua("local s = m.spherical(m.vec3(0, 5, 0)) return s.polar .. ' ' .. s.azimuth .. ' ' .. tostring(s:vector())"), "0.0 0.0 Vec3(0.0, 5.0, 0.0)");
    EXPECT_EQ(lua("local s = m.spherical() s.radius = 4 s.polar = math.pi return tostring(s:vector():approximately({0, -4, 0}, 1e-5))"), "true");
    EXPECT_EQ(lua("local c = m.cylindrical(3, math.pi / 2, 2) return tostring(c:vector():approximately({3, 2, 0}, 1e-6))"), "true");
    EXPECT_EQ(lua("local c = m.cylindrical({0, -4, 0}) return tostring(c)"), "Cylindrical(0.0, 0.0, -4.0)");
    EXPECT_EQ(lua("local c = m.cylindrical({1, 2, 3}) return tostring(c:vector():approximately({1, 2, 3}, 1e-5)) .. ' ' .. c.height"), "true 2.0");
    EXPECT_NE(lua("return m.spherical().bend").find("The type \"haylen.Spherical\" has no member \"bend\"."), std::string::npos);
}

TEST_F(SphericalLuaTest, ProjectsEvaluatesAndConvolvesHarmonics) {
    fixture.runLua("sun = m.sphericalHarmonics() sun:projectDirection(m.up(), {2, 1, 0.5}) irradiance = sun:convolveLambert()");
    EXPECT_EQ(lua("local c = irradiance:evaluate(m.up()) return string.format('%.4f %.4f %.4f %.1f', c.r, c.g, c.b, c.a)"), "2.1250 1.0625 0.5313 1.0");
    EXPECT_EQ(lua("local c = irradiance:evaluate(-m.up()) return string.format('%.4f', c.g)"), "0.0625");
    EXPECT_EQ(lua("local c = sun.coefficients return #c .. ' ' .. string.format('%.6f %.6f', c[1].y, c[7].y)"), "9 0.282095 -0.315392");
    EXPECT_EQ(lua("local twice = sun + sun local tripled = 3 * sun return string.format('%.4f %.4f', twice.coefficients[1].x, tripled.coefficients[1].x) .. ' ' .. tostring(sun * 2 == twice)"), "1.1284 1.6926 true");
    EXPECT_EQ(lua("local dark = m.sphericalHarmonics() return string.format('%.4f', sun:lerp(dark, 0.5).coefficients[1].x)"), "0.2821");
    EXPECT_EQ(lua("local copy = m.sphericalHarmonics(sun.coefficients) return tostring(copy == sun)"), "true");
    EXPECT_NE(lua("return m.sphericalHarmonics({{1, 1, 1}})").find("Spherical harmonics take 9 coefficients, not 1."), std::string::npos);
    EXPECT_NE(lua("return sun:evaluate({0, 0, 0})").find("The vector has no direction, because its length is zero."), std::string::npos);
}

TEST_F(SphericalLuaTest, ProjectsCubemapsAndPanoramasFromFloatBuffers) {
    fixture.runLua("local collections = require('haylen.collections') cube = collections.newFloatBuffer(6 * 4 * 4 * 3, 1) panorama = collections.newFloatBuffer(64 * 32 * 3, 1)");
    EXPECT_EQ(lua("local sky = m.sphericalHarmonicsFromCubemap(cube, 4):convolveLambert() return string.format('%.3f', sky:evaluate(m.forward()).r)"), "3.142");
    EXPECT_EQ(lua("local sky = m.sphericalHarmonicsFromPanorama(panorama, 64, 32):convolveLambert() return string.format('%.2f', sky:evaluate(m.up()).g)"), "3.14");
    EXPECT_NE(lua("return m.sphericalHarmonicsFromCubemap(cube, 3)").find("A cubemap of side 3 holds 162 floats, the red, green and blue of each texel of its 6 faces, not 288."), std::string::npos);
    EXPECT_NE(lua("return m.sphericalHarmonicsFromPanorama(panorama, 0, 32)").find("The width and the height of a panorama must be from 1 to 16384 texels."), std::string::npos);
}

} // namespace haylen::lua::math
