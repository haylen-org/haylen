#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::graphics {

TEST(GradientLuaTest, FillsImagesWithGradients) {
    test::LuaFixture fixture;
    fixture.runLua("graphics = require('haylen.graphics') image = graphics.newImage(4, 4, '#FF000000')");
    EXPECT_EQ(fixture.lua("image:fillGradient({colors = {'#FF000000', '#FFFFFFFF'}, dither = false}) return image:pixel(0, 0):toHex() .. ' ' .. image:pixel(3, 3):toHex()"), "#FF202020 #FFDFDFDF");
    EXPECT_EQ(fixture.lua("image:fillGradient({kind = 'radial', stops = {{0, '#FFFF0000'}, {1, '#000000FF'}}, radius = 0.5, center = {0.5, 0.5}}, {0, 0, 2, 2}) return image:pixel(3, 3):toHex()"), "#FFDFDFDF");
    EXPECT_EQ(fixture.lua("image:fillGradient({kind = 'conic', angle = 1, colors = {'#FFFFFFFF'}}) return image:pixel(1, 1):toHex()"), "#FFFFFFFF");
    EXPECT_NE(fixture.lua("image:fillGradient({})").find("A gradient takes its colors from either \"stops\" or \"colors\"."), std::string::npos);
    EXPECT_NE(fixture.lua("image:fillGradient({colors = {'#FFFFFFFF'}, stops = {}})").find("either"), std::string::npos);
    EXPECT_NE(fixture.lua("image:fillGradient({stops = {{0}}})").find("The stops of a gradient are a list of offset and color pairs"), std::string::npos);
    EXPECT_NE(fixture.lua("image:fillGradient({stops = {{1, '#FFFFFFFF'}, {0, '#FF000000'}}})").find("The stops of a gradient need offsets from 0 to 1 in increasing order."), std::string::npos);
    EXPECT_NE(fixture.lua("image:fillGradient({kind = 'radial', colors = {'#FFFFFFFF'}, radius = 0})").find("A radial gradient needs a finite radius above 0 on both axes."), std::string::npos);
    EXPECT_NE(fixture.lua("image:fillGradient({kind = 'radial', colors = {'#FFFFFFFF'}, radius = math.huge})").find("A radial gradient needs a finite radius above 0 on both axes."), std::string::npos);
    EXPECT_NE(fixture.lua("image:fillGradient({colors = {'#FFFFFFFF'}, angle = 0 / 0})").find("A gradient needs a finite angle."), std::string::npos);
    EXPECT_NE(fixture.lua("image:fillGradient({kind = 'conic', colors = {'#FFFFFFFF'}, center = {math.huge, 0.5}})").find("A gradient needs a center of finite coordinates."), std::string::npos);
    EXPECT_NE(fixture.lua("image:fillGradient({kind = 'diamond', colors = {'#FFFFFFFF'}})").find("diamond"), std::string::npos);
    EXPECT_NE(fixture.lua("image:fillGradient({colors = {'#FFFFFFFF'}, spread = 1})").find("Unknown option \"spread\"."), std::string::npos);
}

} // namespace haylen::lua::graphics
