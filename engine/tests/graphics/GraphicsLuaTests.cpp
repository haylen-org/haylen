#include <gtest/gtest.h>

#include <lua.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::graphics {

TEST(GraphicsLuaTest, CreatesTexturesAndRenderTargets) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(16, 8, 0xFFFFFFFFU);
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
    EXPECT_EQ(fixture.lua("return graphics.apiName()"), "none");
}

// The bytes of an image file, such as a PNG that a plugin returns, become a texture.
TEST(GraphicsLuaTest, CreatesTexturesFromTheBytesOfImageFiles) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(5, 3, 0x336699FFU);
    test::EngineFixture fixture;
    lua_pushlstring(fixture.lua(), reinterpret_cast<const char*>(image.data()), image.size());
    lua_setglobal(fixture.lua(), "png");
    fixture.runLua("graphics = require('haylen.graphics')");

    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(png) return t.width .. 'x' .. t.height .. ' ' .. t.filter"), "5x3 nearest");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(png, {dynamic = true, filter = 'linear', wrap = 'repeat'}) t:update(string.rep('\\0', 60)) return t.filter .. ' ' .. t.wrap"), "linear repeat");
    EXPECT_NE(fixture.lua("graphics.newTexture('not an image')").find("The image could not be decoded"), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newTexture(png, {fill = '#FF0000'})").find("fill"), std::string::npos);
}

// A dynamic texture takes new pixels from Lua, and the frame sends only the last pixels it received.
TEST(GraphicsLuaTest, UpdatesDynamicTextures) {
    test::EngineFixture fixture;
    fixture.runLua("graphics = require('haylen.graphics') minimap = graphics.newTexture(2, 1, {dynamic = true, fill = '#FF0000'})");
    fixture.frames(1);

    fixture.runLua("minimap:update(string.rep(string.char(0, 0, 0, 255), 2)) minimap:update(string.rep(string.char(255, 255, 255, 255), 2))");
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWrites, 1);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWriteBytes, 8U);
    EXPECT_EQ(fixture.lua("return require('haylen.graphics2d').stats().uploadedBytes"), "8");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(1, 1, {dynamic = true, pixels = string.char(1, 2, 3, 4)}) return t.width"), "1");
    EXPECT_NE(fixture.lua("minimap:update('short')").find("The pixels do not match the size of the dynamic texture."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newTexture(1, 1):update(string.char(1, 2, 3, 4))").find("Only a dynamic texture changes its pixels in place."), std::string::npos);
}

// Images keep pixels on the CPU, decode and blit on the task pool, pack into atlases and become textures, and work on the pool never sees what Lua changes later.
TEST(GraphicsLuaTest, EditsImagesAndPacksThemIntoAtlases) {
    const std::vector<std::uint8_t> png = test::TestFiles::pngImage(6, 4, 0x336699FFU);
    test::EngineFixture fixture({{"content/images/tile.png", std::string(png.begin(), png.end())}});
    lua_pushlstring(fixture.lua(), reinterpret_cast<const char*>(png.data()), png.size());
    lua_setglobal(fixture.lua(), "png");
    // clang-format off
    fixture.runLua(R"(
        graphics = require('haylen.graphics')
        animation2d = require('haylen.animation2d')
        assets = require('haylen.assets')
        async = require('async')
        canvas = graphics.newImage(16, 8)
        canvas:fill('#FF0000', {0, 0, 4, 4})
        tile = assets.load('images/tile.png', 'image')
        canvas:blit(tile, 8, 2, {source = {0, 0, 4, 4}})
        canvas:setPixel(15, 7, '#8000FF00')
        canvas:blit(canvas, 12, 0, {source = {0, 0, 2, 2}, blend = true})
        async.spawn(function()
            local decoded = graphics.decodeImageAsync(png):await()
            local stamped = canvas:blitAsync(decoded, 0, 4):await()
            canvas:setPixel(0, 4, '#FFFFFF')
            local builder = animation2d.newAtlasBuilder()
            builder:add('canvas', stamped)
            builder:add('corner', stamped, {0, 0, 4, 4})
            local atlas = builder:buildAsync({padding = 2, extrude = 1, filter = 'linear', mipmaps = true}):await()
            done = decoded.width .. ' ' .. stamped:pixel(0, 4):toHex() .. ' ' .. canvas:pixel(0, 4):toHex() .. ' ' .. builder.count .. ' ' .. atlas:frame('corner').source.width .. ' ' .. tostring(atlas.texture.mipmaps)
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return done") != "nil" || fixture.engine().getError() != nullptr; }));
    ASSERT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
    EXPECT_EQ(fixture.lua("return done"), "6 #FF336699 #FFFFFFFF 2 4.0 true");
    EXPECT_EQ(fixture.lua("return canvas.width .. 'x' .. canvas.height .. ' ' .. canvas:pixel(1, 1):toHex() .. ' ' .. canvas:pixel(9, 3):toHex() .. ' ' .. canvas:pixel(13, 1):toHex()"), "16x8 #FFFF0000 #FF336699 #FFFF0000");
    EXPECT_EQ(fixture.lua("local area = canvas:visibleArea() return area.width .. ' ' .. tostring(graphics.newImage(2, 2):visibleArea())"), "16.0 nil");
    EXPECT_EQ(fixture.lua("local part = canvas:crop({8, 2, 4, 4}) part:setPixel(0, 0, '#000000') return part.width .. ' ' .. canvas:pixel(8, 2):toHex() .. ' ' .. graphics.decodeImage(png).height"), "4 #FF336699 4");
    EXPECT_EQ(fixture.lua("local copy = tile:clone() copy:fill('#000000') return assets.load('images/tile.png', 'image'):pixel(0, 0):toHex() .. ' ' .. copy:pixel(0, 0):toHex()"), "#FF336699 #FF000000");
    EXPECT_EQ(fixture.lua("tile:fill('#000000') return assets.load('images/tile.png', 'image'):pixel(0, 0):toHex()"), "#FF336699");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(canvas, {filter = 'linear', mipmaps = true}) return t.width .. ' ' .. tostring(t.mipmaps) .. ' ' .. tostring(graphics.newTexture(2, 2).mipmaps)"), "16 true false");
    EXPECT_EQ(fixture.lua("local builder = animation2d.newAtlasBuilder() builder:add('a', canvas) builder:add('b', tile) local atlas = builder:build({trim = false}) return atlas:frame('a').source.width .. ' ' .. #atlas:frameNames() .. ' ' .. #atlas:subset('b'):frameNames()"), "16.0 2 1");
    EXPECT_EQ(fixture.lua("canvas:bleed() return canvas:pixel(15, 0).a"), "0.0");

    EXPECT_NE(fixture.lua("graphics.newImage(0, 4)").find("An image needs a width and a height of one pixel or more."), std::string::npos);
    EXPECT_NE(fixture.lua("canvas:crop({10, 4, 8, 8})").find("is not inside the image of 16 by 8 pixels."), std::string::npos);
    EXPECT_NE(fixture.lua("canvas:blit(tile, 0, 0, {mode = 'add'})").find("Unknown option \"mode\"."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.decodeImage('not an image')").find("The image could not be decoded"), std::string::npos);
    EXPECT_NE(fixture.lua("animation2d.newAtlasBuilder():build()").find("An atlas needs at least one image."), std::string::npos);
    EXPECT_NE(fixture.lua("local b = animation2d.newAtlasBuilder() b:add('a', canvas) b:add('a', canvas)").find("The atlas has the frame \"a\" twice."), std::string::npos);
    EXPECT_NE(fixture.lua("local b = animation2d.newAtlasBuilder() b:add('a', canvas) b:build({maxSize = 8})").find("need more room than a texture of 8 pixels."), std::string::npos);
    EXPECT_NE(fixture.lua("local b = animation2d.newAtlasBuilder() b:add('a', canvas) b:build({padding = 1, extrude = 1})").find("An atlas needs a padding of zero or more"), std::string::npos);
    EXPECT_NE(fixture.lua("local b = animation2d.newAtlasBuilder() b:add('a', canvas) b:build({rotate = true})").find("Unknown option \"rotate\"."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newTexture(canvas, {dynamic = true})").find("Unknown option \"dynamic\"."), std::string::npos);
}

// Render targets and whole frames come back as images in later frames, and images become PNG files.
TEST(GraphicsLuaTest, ReadsTargetsCapturesTheScreenAndEncodesPngFiles) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        graphics = require('haylen.graphics')
        async = require('async')
        target = graphics.newRenderTarget(6, 3)
        async.spawn(function()
            local read = graphics.readTarget(target):await()
            local screen = graphics.captureScreen():await()
            local image = graphics.newImage(4, 2, '#FF336699')
            local png = image:encodePngAsync():await()
            done = read.width .. 'x' .. read.height .. ' ' .. tostring(screen.width > 0) .. ' ' .. graphics.decodeImage(png):pixel(3, 1):toHex() .. ' ' .. tostring(png == image:encodePng())
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return done") != "nil" || fixture.engine().getError() != nullptr; }));
    ASSERT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
    EXPECT_EQ(fixture.lua("return done"), "6x3 true #FF336699 true");
    EXPECT_NE(fixture.lua("graphics.readTarget(5)").find("haylen.RenderTarget expected"), std::string::npos);
}

} // namespace haylen::graphics
