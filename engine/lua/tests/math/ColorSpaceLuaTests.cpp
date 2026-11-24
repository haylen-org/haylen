#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

class ColorSpaceLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("m = require('haylen.math') space = m.colorSpace light = m.photometry");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture;
};

TEST_F(ColorSpaceLuaTest, ConvertsColorsBetweenSrgbAndLinearLight) {
    EXPECT_EQ(lua("return string.format('%.4f %.2f', m.color(0.5, 0.5, 0.5, 0.25):linear().r, m.color(0.5, 0.5, 0.5, 0.25):linear().a)"), "0.2140 0.25");
    EXPECT_EQ(lua("return tostring(m.fromLinear(m.color('#FF808080'):linear()))"), "#FF808080");
    EXPECT_EQ(lua("local hdr = m.fromLinear({4, 1, 0}) return string.format('%.3f %.3f', hdr:linear().r, hdr:luminance() > 1 and 1 or 0)"), "4.000 1.000");
    EXPECT_EQ(lua("local c = 2 * m.color(0.5, 1, 2, 0.5) * 2 return string.format('%.1f %.1f %.1f %.1f', c.r, c.g, c.b, c.a)"), "2.0 4.0 8.0 0.5");
    EXPECT_EQ(lua("return tostring(m.color('#FFFFFFFF') * m.color(0.5, 0.5, 1))"), "#FF8080FF");
}

TEST_F(ColorSpaceLuaTest, MakesTheColorOfATemperature) {
    EXPECT_EQ(lua("local warm = m.fromTemperature(2700) return string.format('%.1f %.2f', warm.r, warm.g)"), "1.0 0.68");
    EXPECT_EQ(lua("local sky = m.fromTemperature(10000) return tostring(sky.b > sky.r)"), "true");
    EXPECT_NE(lua("return m.fromTemperature(800)").find("A color temperature must be from 1667 to 25000 kelvin."), std::string::npos);
}

TEST_F(ColorSpaceLuaTest, EncodesForEveryScreen) {
    EXPECT_EQ(lua("return tostring(space.encode(m.white(), 'srgb'))"), "#FFFFFFFF");
    EXPECT_EQ(lua("local signal = space.encode({0.5, 0.2, 2, 0.75}, 'hdr10') local back = space.decode(signal, 'hdr10') return string.format('%.2f %.2f %.2f %.2f', back.r, back.g, back.b, back.a)"), "0.50 0.20 2.00 0.75");
    EXPECT_EQ(lua("local low = space.encode(m.white(), 'hdr10') local high = space.encode(m.white(), 'hdr10', {referenceWhite = 400}) return tostring(high.g > low.g)"), "true");
    EXPECT_EQ(lua("return string.format('%.4f %.0f %.0f', space.encodePq(space.referenceWhite), space.decodePq(1), space.peakNits)"), "0.5807 10000 10000");
    EXPECT_EQ(lua("return string.format('%.5f %.5f', space.decodeSrgb(0.5), space.encodeSrgb(space.decodeSrgb(0.5)))"), "0.21404 0.50000");
    EXPECT_NE(lua("return space.encode(m.white(), 'hdr10', {referenceWhite = 0})").find("The reference white must be a brightness above 0 and at most 10000 nits."), std::string::npos);
    EXPECT_NE(lua("return space.encode(m.white(), 'hdr10', {white = 100})").find("Unknown option \"white\"."), std::string::npos);
    EXPECT_NE(lua("return space.encode(m.white(), 'rec2020')").find("rec2020"), std::string::npos);
}

TEST_F(ColorSpaceLuaTest, MovesColorsBetweenPrimaries) {
    EXPECT_EQ(lua("local red = space.convert({1, 0, 0}, 'displayP3', 'srgb') return tostring(red.r > 1 and red.g < 0)"), "true");
    EXPECT_EQ(lua("local xyz = space.toXyz(m.white(), 'displayP3') return string.format('%.3f %.3f', xyz.y, space.luminance(m.white(), 'hdr10'))"), "1.000 1.000");
    EXPECT_EQ(lua("local back = space.fromXyz(space.toXyz({0.2, 0.4, 0.6}, 'srgb'), 'srgb') return string.format('%.3f %.3f %.3f', back.r, back.g, back.b)"), "0.200 0.400 0.600");
}

TEST_F(ColorSpaceLuaTest, ConvertsThePhysicalUnitsOfLight) {
    EXPECT_EQ(lua("local candelas = light.pointIntensity(800) return string.format('%.2f %.2f', candelas, light.illuminance(candelas, 2))"), "63.66 15.92");
    EXPECT_EQ(lua("return string.format('%.2f', light.spotIntensity(800, math.pi / 2))"), "127.32");
    EXPECT_EQ(lua("local ev = light.ev100FromCamera(16, 1 / 100, 100) return string.format('%.2f %.4f %.1f', ev, light.exposure(0), light.ev100FromLuminance(0.125))"), "14.64 0.8333 0.0");
    EXPECT_NE(lua("return light.illuminance(10, 0)").find("The distance to a light must be above 0 meters."), std::string::npos);
    EXPECT_NE(lua("return light.spotIntensity(800, 0)").find("The cone of a spot light needs an outer angle above 0 and at most pi radians."), std::string::npos);
}

} // namespace haylen::lua::math
