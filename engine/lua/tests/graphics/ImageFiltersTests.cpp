#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::graphics {

TEST(ImageFiltersLuaTest, AppliesEffectsInPlaceAndOnTheTaskPool) {
    test::LuaFixture fixture;
    fixture.runLua("graphics = require('haylen.graphics') async = require('async') image = graphics.newImage(4, 4, '#FF0A141E')");
    EXPECT_EQ(fixture.lua("image:invert() return image:pixel(0, 0):toHex()"), "#FFF5EBE1");
    EXPECT_EQ(fixture.lua("image:adjust({saturation = 0}) image:blur(1) image:grayscale() image:tint('#FF000000', 0) image:pixelate(2) image:sharpen() image:outline('#FF000000', 1) return image.width"), "4");
    // clang-format off
    fixture.runLua(R"(
        async.spawn(function()
            local result = image:applyAsync({{effect = 'invert'}, {effect = 'pixelate', size = 2}}):await()
            done = result:pixel(0, 0):toHex() .. ' ' .. image:pixel(0, 0):toHex()
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(done)") != "nil"; }));
    EXPECT_EQ(fixture.lua("local inverted, original = done:match('(%S+) (%S+)') return tostring(inverted ~= original)"), "true");
    EXPECT_NE(fixture.lua("image:applyAsync({{effect = 'melt'}})").find("The image effect \"melt\" is unknown."), std::string::npos);
    EXPECT_NE(fixture.lua("image:applyAsync({{effect = 'blur', radius = 2, sigma = 1}})").find("Unknown option \"sigma\"."), std::string::npos);
    EXPECT_NE(fixture.lua("image:applyAsync({{effect = 'pixelate', size = 1e20}})").find("size"), std::string::npos);
    EXPECT_NE(fixture.lua("image:adjust({gamma = 2})").find("Unknown option \"gamma\"."), std::string::npos);
    EXPECT_NE(fixture.lua("image:blur(-1)").find("A blur has a radius from 0 to 64 pixels."), std::string::npos);
    EXPECT_NE(fixture.lua("image:adjust({contrast = 0 / 0})").find("An adjustment of the colors has a finite brightness, contrast, saturation and hue."), std::string::npos);
    EXPECT_NE(fixture.lua("image:grayscale(math.huge)").find("A grayscale has a finite amount."), std::string::npos);
    EXPECT_NE(fixture.lua("image:tint('#FF000000', 0 / 0)").find("A tint has a finite amount and a color of finite channels."), std::string::npos);
}

} // namespace haylen::lua::graphics
