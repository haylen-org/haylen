#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::graphics2d {

// The path of Lua chains its commands, fills and strokes itself at a place with a rotation and a scale, gives its outlines as a shape and names what it cannot take.
TEST(PathLuaTest, BuildsFillsStrokesAndFlattensPaths) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        graphics2d = require('haylen.graphics2d')
        path = graphics2d.newPath()
        empty = path.empty
        path:moveTo(0, 0):lineTo(100, 0):quadraticTo(150, 50, 100, 100):cubicTo(60, 140, 40, 60, 0, 100):close()
        path:arc(50, 50, 20, 0, -2 * math.pi):close()
        require('haylen.scene').push({render = function()
            graphics2d.beginScreen()
            path:fill(10, 20, {color = '#FF3080FF', fillRule = 'evenOdd', rotation = 0.5, scaleX = 2, layer = 1})
            path:stroke(10, 20, {width = 4, join = 'round', cap = 'round', dashes = {8, 4}, color = '#FFFFFFFF', scaleY = 2, layer = 1})
        end})
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.lua("return tostring(empty) .. ' ' .. tostring(path.empty)"), "true false");
    EXPECT_EQ(fixture.lua("local shape, closed = path:outlines(1) return #shape .. ' ' .. tostring(closed[1]) .. ' ' .. shape[1][2].x .. ' ' .. tostring(#shape[2] > 8)"), "2 true 100.0 true");
    EXPECT_EQ(fixture.lua("return #graphics2d.newPath():moveTo(0, 0):lineTo(5, 5):outlines()"), "1");

    EXPECT_NE(fixture.lua("graphics2d.newPath():lineTo(1, 1)").find("A path has no point to continue from, so \"moveTo\" comes before its first line or curve."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.newPath():close()").find("A path closes only an outline it has open"), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.newPath():arc(0, 0, -1, 0, 1)").find("An arc of a path takes a finite radius of zero or more and finite angles."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.beginScreen() path:fill(0, 0, {rule = 'evenOdd'})").find("Unknown option \"rule\"."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.beginScreen() path:fill(0, 0, {fillRule = 'odd'})").find("fillRule"), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.beginScreen() path:stroke(0, 0, {widths = {1, 2}})").find("Unknown option \"widths\"."), std::string::npos);
    EXPECT_NE(fixture.lua("path:outlines(0)").find("A path flattens within a positive and finite tolerance."), std::string::npos);
}

} // namespace haylen::lua::graphics2d
