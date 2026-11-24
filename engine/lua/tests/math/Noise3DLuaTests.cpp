#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

class Noise3DLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("m = require('haylen.math') noise = m.noise3d(7)");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture;
};

TEST_F(Noise3DLuaTest, SamplesEveryNoiseInThreeAndFourDimensions) {
    EXPECT_EQ(lua("return noise:perlin(4, -2, 9) .. ' ' .. noise:perlin(4, -2, 9, 1)"), "0.0 0.0");
    EXPECT_EQ(lua("local a, b = noise:simplex(1.3, 2.7, -0.4), m.noise3d(7):simplex(1.3, 2.7, -0.4) return tostring(a == b and a >= -1 and a <= 1)"), "true");
    EXPECT_EQ(lua("local v = noise:value(0.5, 0.5, 0.5, 0.5) return tostring(v >= -1 and v <= 1)"), "true");
    EXPECT_EQ(lua("local nearest, second, cell = noise:cellular(0.2, 3.1, 5.5) return tostring(nearest <= second and cell >= 0 and cell < 1)"), "true");
    EXPECT_EQ(lua("local near4 = noise:cellular(0.2, 3.1, 5.5, 2) return tostring(near4 >= 0)"), "true");
}

TEST_F(Noise3DLuaTest, SumsFractalsAndWarpsPoints) {
    EXPECT_EQ(lua("return tostring(noise:fractal(1.3, 2.7, -0.4, {octaves = 1}) == noise:simplex(1.3, 2.7, -0.4))"), "true");
    EXPECT_EQ(lua("local f = noise:fractal(1, 2, 3, 4, {basis = 'perlin', fractal = 'ridged', octaves = 5, gain = 0.6}) return tostring(f >= -1 and f <= 1)"), "true");
    EXPECT_EQ(lua("local x, y, z = noise:warp(1, 2, 3, 0) return x .. ' ' .. y .. ' ' .. z"), "1.0 2.0 3.0");
    EXPECT_NE(lua("return noise:fractal(1, 2, 3, {octaves = 40})").find("The octaves must be a whole number from 1 to 16."), std::string::npos);
    EXPECT_NE(lua("return noise:fractal(1, 2, 3, {fractal = 'wavy'})").find("wavy"), std::string::npos);
    EXPECT_NE(lua("return noise:fractal(1, 2, 3, {bend = 1})").find("Unknown option \"bend\"."), std::string::npos);
}

TEST_F(Noise3DLuaTest, SpreadsRandomPointsAndPoissonPointsIn3D) {
    fixture.runLua("random = m.random(3)");
    EXPECT_EQ(lua("return string.format('%.3f', random:onSphere(2):length())"), "2.000");
    EXPECT_EQ(lua("return tostring(random:inSphere():length() <= 1 and random:inBox(m.box3({0, 0, 0}, {1, 2, 3})).y <= 2)"), "true");
    EXPECT_EQ(lua("return tostring(random:inCone({0, 1, 0}, 0.2):dot({0, 1, 0}) >= math.cos(0.2) - 1e-4)"), "true");
    EXPECT_NE(lua("return random:inCone({0, 0, 0}, 0.2)").find("The vector has no direction, because its length is zero."), std::string::npos);
    EXPECT_NE(lua("return random:inCone({0, 1, 0}, 5)").find("A random cone needs an angle from 0 to pi radians."), std::string::npos);
    EXPECT_EQ(lua("local points = m.poissonDisk3d({volume = m.box3({0, 0, 0}, {6, 6, 6}), minimumDistance = 1.5, seed = 2}) local again = m.poissonDisk3d({volume = m.box3({0, 0, 0}, {6, 6, 6}), minimumDistance = 1.5, seed = 2}) return tostring(#points > 20 and #points == #again and points[1] == again[1])"), "true");
    EXPECT_EQ(lua("local points = m.poissonDisk3d({volume = m.box3({0, 0, 0}, {6, 6, 6}), minimumDistance = 2, accept = function(point) return point.y > 3 end}) for _, point in ipairs(points) do if point.y <= 3 then return 'below' end end return 'above'"), "above");
    EXPECT_NE(lua("return m.poissonDisk3d({volume = m.box3({0, 0, 0}, {6, 6, 6}), attempts = 0})").find("Poisson sampling needs between 1 and 1000 attempts."), std::string::npos);
}

} // namespace haylen::lua::math
