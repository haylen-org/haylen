#include <gtest/gtest.h>

#include <string>

#include "support/EngineFixture.hpp"

namespace haylen::graphics {

TEST(GraphicsLuaTest, CreatesTexturesAndRenderTargets) {
    const std::vector<std::uint8_t> image = test::pngImage(16, 8, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/images/hero.png", std::string(image.begin(), image.end())}});
    fixture.runLua("graphics = require('haylen.graphics') hero = require('haylen.assets').texture('images/hero.png')");

    EXPECT_EQ(fixture.lua("return hero.width .. 'x' .. hero.height"), "16x8");
    EXPECT_EQ(fixture.lua("return hero == require('haylen.assets').texture('images/hero.png') and hero ~= graphics.whiteTexture()"), "true");
    EXPECT_EQ(fixture.lua("local white = graphics.whiteTexture() return white.width .. 'x' .. white.height .. ' ' .. tostring(white == graphics.whiteTexture())"), "1x1 true");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(4, 2, {fill = '#FF0000', filter = 'linear'}) return t.width .. 'x' .. t.height .. ' ' .. t.filter .. ' ' .. t.wrap"), "4x2 linear clamp");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(1, 1, {wrap = 'mirror'}) return t.filter .. ' ' .. t.wrap"), "nearest mirror");
    EXPECT_NE(fixture.lua("return graphics.newTexture(2, 2, 5)").find("bad argument #3 to 'newTexture' (table expected, got number)"), std::string::npos);
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(1, 1, {pixels = string.char(255, 0, 0, 255)}) return t.width"), "1");
    EXPECT_NE(fixture.lua("return graphics.newTexture(2, 2, {pixels = 'short'})").find("error: "), std::string::npos);
    EXPECT_EQ(fixture.lua("return graphics.maxTextureSize()"), "16384");

    // A size beyond the device fails before its pixels are allocated.
    EXPECT_NE(fixture.lua("return graphics.newTexture(2147483647, 2147483647)").find("Texture dimensions exceed the device limit."), std::string::npos);
    EXPECT_EQ(fixture.lua("local target = graphics.newRenderTarget(64, 32, {wrap = 'repeat'}) return target.width .. 'x' .. target.height .. ' ' .. target.texture.width"), "64x32 64");
    EXPECT_NE(fixture.lua("return graphics.newRenderTarget(0, 32)").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newRenderTarget(4, 4, {filter = 'blurry'})").find("error: "), std::string::npos);
    EXPECT_EQ(fixture.lua("return graphics.backend()"), "dummy");
}

} // namespace haylen::graphics
