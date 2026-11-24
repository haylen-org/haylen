#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

TEST(ColorLuaTest, ReadsLuminanceContrastAndTheContrastingColor) {
    test::LuaFixture fixture;
    fixture.runLua("m = require('haylen.math') night = m.color('#FF1E2A44')");
    EXPECT_EQ(fixture.lua("return tostring(night:dark()) .. ' ' .. night:contrastingColor():toHex() .. ' ' .. string.format('%.1f', m.contrast(m.black(), m.white()))"), "true #FFFFFFFF 21.0");
    EXPECT_EQ(fixture.lua("return string.format('%.3f', m.white():luminance())"), "1.000");
}

TEST(ColorLuaTest, ConvertsToHsvAndBlendsAroundTheHueCircle) {
    test::LuaFixture fixture;
    fixture.runLua("m = require('haylen.math')");

    EXPECT_EQ(fixture.lua("return string.format('%.3f %.1f %.1f %.2f', m.color(0, 0, 1, 0.25):toHsv())"), "0.667 1.0 1.0 0.25");
    // Red to blue passes through magenta in HSV instead of the dark purple of a plain lerp.
    EXPECT_EQ(fixture.lua("local red, blue = m.color('#FFFF0000'), m.color('#FF0000FF') return tostring(red:lerpHsv(blue, 0.5)) .. ' ' .. tostring(red:lerp(blue, 0.5))"), "#FFFF00FF #FF800080");
    // A gray takes the hue of the other color, so only the saturation changes, whichever end it is.
    EXPECT_EQ(fixture.lua("return tostring(m.color(1, 1, 1):lerpHsv({1, 0, 0}, 0.5))"), "#FFFF8080");
    EXPECT_EQ(fixture.lua("return tostring(m.color(1, 0, 0):lerpHsv({1, 1, 1}, 0.5))"), "#FFFF8080");
}

TEST(Transform2DLuaTest, RaisesAPlainErrorForAnInverseThatDoesNotExist) {
    test::LuaFixture fixture;
    fixture.runLua("m = require('haylen.math')");
    EXPECT_EQ(fixture.lua("return tostring(m.scaling({0, 1}):isInvertible()) .. ' ' .. tostring(m.scaling({2, 2}):isInvertible())"), "false true");
    EXPECT_NE(fixture.lua("return m.scaling({0, 1}):inverse()").find("The transform has no inverse, because it squashes the plane flat and its determinant is zero."), std::string::npos);
    EXPECT_NE(fixture.lua("return m.vec2(0, 0):normalized()").find("The vector has no direction, because its length is zero."), std::string::npos);
    EXPECT_EQ(fixture.lua("return tostring(m.vec2(1e-20, 0):normalized())"), "Vec2(1.0, 0.0)");
}

} // namespace haylen::lua::math
