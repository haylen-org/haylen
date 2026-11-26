#include <gtest/gtest.h>

#include <lua.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "support/LuaFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::graphics {

TEST(GraphicsLuaTest, CreatesTexturesAndRenderTargets) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(16, 8, 0xFFFFFFFFU);
    test::LuaFixture fixture({{"content/images/hero.png", std::string(image.begin(), image.end())}});
    fixture.runLua("graphics = require('haylen.graphics') hero = require('haylen.assets').texture('images/hero.png')");

    EXPECT_EQ(fixture.lua("return hero.width .. 'x' .. hero.height"), "16x8");
    EXPECT_EQ(fixture.lua("return hero == require('haylen.assets').texture('images/hero.png') and hero ~= graphics.whiteTexture()"), "true");
    EXPECT_EQ(fixture.lua("local white = graphics.whiteTexture() return white.width .. 'x' .. white.height .. ' ' .. tostring(white == graphics.whiteTexture())"), "1x1 true");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(4, 2, {fill = '#FF0000', filter = 'linear'}) return t.width .. 'x' .. t.height .. ' ' .. t.filter .. ' ' .. t.wrapU"), "4x2 linear clamp");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(1, 1, {wrap = 'mirror'}) return t.filter .. ' ' .. t.wrapV"), "nearest mirror");
    EXPECT_NE(fixture.lua("return graphics.newTexture(2, 2, 5)").find("bad argument #3 to 'newTexture' (table expected, got number)"), std::string::npos);
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(1, 1, {pixels = string.char(255, 0, 0, 255)}) return t.width"), "1");
    EXPECT_NE(fixture.lua("return graphics.newTexture(2, 2, {pixels = 'short'})").find("error: "), std::string::npos);
    EXPECT_EQ(fixture.lua("return graphics.maxTextureSize()"), "16384");
    EXPECT_EQ(fixture.lua("return graphics.depthRange()"), "zeroToOne");

    // A size beyond the device fails before its pixels are allocated.
    EXPECT_NE(fixture.lua("return graphics.newTexture(2147483647, 2147483647)").find("Texture dimensions exceed the device limit."), std::string::npos);
    EXPECT_EQ(fixture.lua("local target = graphics.newRenderTarget(64, 32, {wrap = 'repeat'}) return target.width .. 'x' .. target.height .. ' ' .. target.texture.width"), "64x32 64");
    EXPECT_NE(fixture.lua("return graphics.newRenderTarget(0, 32)").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newRenderTarget(4, 4, {filter = 'blurry'})").find("error: "), std::string::npos);
    EXPECT_EQ(fixture.lua("return graphics.apiName()"), "none");
}

// The capabilities of the device and what it does with each pixel format reach Lua as tables.
TEST(GraphicsLuaTest, ReportsTheCapabilitiesOfTheDeviceAndOfEachFormat) {
    test::LuaFixture fixture;
    fixture.runLua("graphics = require('haylen.graphics') capabilities = graphics.capabilities()");

    EXPECT_EQ(fixture.lua("return capabilities.maxTextureSize .. ' ' .. capabilities.maxArrayLayers .. ' ' .. capabilities.maxVolumeSize .. ' ' .. capabilities.maxCubeSize"), "16384 2048 2048 16384");
    EXPECT_EQ(fixture.lua("return tostring(capabilities.originTopLeft) .. ' ' .. capabilities.depthRange .. ' ' .. tostring(capabilities.independentBlend) .. ' ' .. tostring(capabilities.gpuTiming)"), "true zeroToOne false false");
    EXPECT_EQ(fixture.lua("local s = graphics.formatSupport('rgba16Float') return tostring(s.sample) .. tostring(s.filter) .. tostring(s.render) .. tostring(s.blend) .. ' ' .. table.concat(s.sampleCounts, ',')"), "truetruetruetrue 1,2,4,8");
    EXPECT_EQ(fixture.lua("local s = graphics.formatSupport('rgba32Float') return tostring(s.filter) .. ' ' .. tostring(s.blend) .. ' ' .. tostring(s.storage)"), "false false true");
    EXPECT_EQ(fixture.lua("local s = graphics.formatSupport('astc6x6Srgb') return tostring(s.sample) .. ' ' .. tostring(s.render) .. ' ' .. #s.sampleCounts"), "true false 0");
    EXPECT_EQ(fixture.lua("local s = graphics.formatSupport('depth32Float') return tostring(s.render) .. ' ' .. tostring(s.filter)"), "true false");
    EXPECT_NE(fixture.lua("return graphics.formatSupport('rgba7')").find("unknown value 'rgba7'"), std::string::npos);
}

// The sampling of a texture follows its options on each axis, with anisotropy, levels of detail, a bias and a border color that the device checks.
TEST(GraphicsLuaTest, SamplesTexturesAsTheirOptionsSay) {
    test::LuaFixture fixture;
    fixture.runLua("graphics = require('haylen.graphics')");

    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(4, 4, {wrap = 'repeat', wrapV = 'mirror'}) return t.wrapU .. ' ' .. t.wrapV .. ' ' .. t.wrapW"), "repeat mirror repeat");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(4, 4, {wrapV = 'border', borderColor = 'opaqueWhite'}) return t.wrapU .. ' ' .. t.wrapV .. ' ' .. t.borderColor"), "clamp border opaqueWhite");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(4, 4, {filter = 'linear'}) return t.anisotropy"), "16");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(4, 4, {filter = 'linear', anisotropy = 4}) return t.anisotropy"), "4");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(4, 4) return t.anisotropy .. ' ' .. t.lodBias .. ' ' .. t.minLod .. ' ' .. t.maxLod"), "1 0.0 0.0 1000.0");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(4, 4, {filter = 'linear', lodBias = -1, minLod = 1, maxLod = 2}) return t.lodBias .. ' ' .. t.minLod .. ' ' .. t.maxLod"), "-1.0 1.0 2.0");
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {filter = 'linear', anisotropy = 32})").find("The texture option \"anisotropy\" takes from 1 to 16 samples on this device, not 32."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {anisotropy = 4})").find("Anisotropic filtering takes the filter \"linear\" and mipmaps."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {minLod = 3, maxLod = 2})").find("The levels of detail of a texture run from \"minLod\" up to \"maxLod\", from 0 on."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {lodBias = 20})").find("The bias of the level of detail of a texture lies within 16"), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {borderColor = 'gray'})").find("gray"), std::string::npos);
    EXPECT_EQ(fixture.lua("local c = graphics.capabilities() return c.maxAnisotropy .. ' ' .. c.maxLodBias .. ' ' .. tostring(c.borderColors)"), "16 16.0 true");
}

// A texture made in a format takes the bytes of each of its levels as they are, such as the blocks of a compressed format, and refuses levels that do not fit.
TEST(GraphicsLuaTest, CreatesTexturesFromTheLevelsOfAFormat) {
    test::LuaFixture fixture;
    fixture.runLua("graphics = require('haylen.graphics') block = string.rep('\\0', 16)");

    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(4, 4, {format = 'bc7', levels = {block}, filter = 'linear'}) return t.format .. ' ' .. t.width .. ' ' .. tostring(t.mipmaps)"), "bc7 4 false");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(8, 8, {format = 'bc7', levels = {string.rep(block, 4), block}}) return tostring(t.mipmaps)"), "true");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(10, 8, {format = 'astc5x4', levels = {string.rep(block, 4)}}) return t.format"), "astc5x4");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(2, 1, {format = 'rgba8', levels = {string.rep('\\255', 8)}}) return t.format"), "rgba8");
    EXPECT_EQ(fixture.lua("return graphics.whiteTexture().format"), "rgba8");
    EXPECT_NE(fixture.lua("return graphics.newTexture(6, 4, {format = 'bc1', levels = {block}})").find("A texture of the format \"bc1\" has sides of whole blocks of 4 by 4 pixels, which 6 by 4 pixels are not."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {format = 'bc1', levels = {block}})").find("The level 1 of a texture of 4 by 4 pixels of the format \"bc1\" holds 16 bytes, and it needs 8."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {format = 'bc7', levels = {block}, mipmaps = true})").find("A texture made from its levels keeps mipmaps only when it brings more than one level."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {format = 'depth16', levels = {block}})").find("The format \"depth16\" belongs to depth attachments, so no texture is made in it."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {format = 'bc7'})").find("A texture made in a format takes the bytes of each of its levels in the list \"levels\", the largest level first."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {format = 'bc7', levels = {}})").find("A texture of 4 by 4 pixels brings from 1 to 3 levels, not 0."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {format = 'bc7', levels = {block}, fill = '#FF0000'})").find("Unknown option \"fill\"."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {format = 'bc9', levels = {block}})").find("bc9"), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(1, 1, {format = 'rgba32Float', levels = {string.rep('\\0', 16)}})").find("The graphics API \"none\" of this device samples the format \"rgba32Float\" only without filtering, and textures filter."), std::string::npos);
}

// The memory of the textures that the app let go of stays with the device for later until a trim gives it back, which the statistics report.
TEST(GraphicsLuaTest, TrimsTheMemoryThatTheDeviceHoldsForLater) {
    test::LuaFixture fixture;
    fixture.runLua("graphics = require('haylen.graphics') debugging = require('haylen.debug') textures = {} for index = 1, 4 do textures[index] = graphics.newTexture(32, 32) end");
    fixture.frames(1);
    fixture.runLua("textures = nil collectgarbage() collectgarbage()");
    fixture.frames(1);

    EXPECT_EQ(fixture.lua("return tostring(debugging.stats().memory.gpuSpare > 0)"), "true");
    const int trims = fixture.host().getGpu().getTrimCount();
    EXPECT_EQ(fixture.lua("graphics.trim() return debugging.stats().memory.gpuSpare"), "0");
    EXPECT_EQ(fixture.host().getGpu().getTrimCount(), trims + 1);
}

// The bytes of an image file, such as a PNG that a plugin returns, become a texture.
TEST(GraphicsLuaTest, CreatesTexturesFromTheBytesOfImageFiles) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(5, 3, 0x336699FFU);
    test::LuaFixture fixture;
    lua_pushlstring(fixture.lua(), reinterpret_cast<const char*>(image.data()), image.size());
    lua_setglobal(fixture.lua(), "png");
    fixture.runLua("graphics = require('haylen.graphics')");

    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(png) return t.width .. 'x' .. t.height .. ' ' .. t.filter"), "5x3 nearest");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(png, {dynamic = true, filter = 'linear', wrap = 'repeat'}) t:update(string.rep('\\0', 60)) return t.filter .. ' ' .. t.wrapU"), "linear repeat");
    EXPECT_NE(fixture.lua("graphics.newTexture('not an image')").find("The image is damaged, or it is not a PNG, JPEG, TGA, BMP or GIF file."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newTexture(png, {fill = '#FF0000'})").find("fill"), std::string::npos);
}

// A dynamic texture takes new pixels from Lua, and the frame sends only the last pixels it received.
TEST(GraphicsLuaTest, UpdatesDynamicTextures) {
    test::LuaFixture fixture;
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

    // A region says which pixels changed, and the frame sends only those.
    fixture.frames(1);
    fixture.runLua("minimap:update(string.rep(string.char(0, 0, 255, 255), 2), {region = {1, 0, 1, 1}})");
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWriteBytes, 4U);
    EXPECT_EQ(fixture.lua("minimap:update(string.rep('\\0', 8), {region = {x = 0, y = 0, width = 2, height = 1}}) return 'ok'"), "ok");
    EXPECT_NE(fixture.lua("minimap:update(string.rep('\\0', 8), {region = {1, 0, 2, 1}})").find("The region of a texture update must lie inside the dynamic texture and hold at least one pixel."), std::string::npos);
    EXPECT_NE(fixture.lua("minimap:update(string.rep('\\0', 8), {region = {0.5, 0, 1, 1}})").find("The region of \"texture:update\" must hold whole pixels."), std::string::npos);
    EXPECT_NE(fixture.lua("minimap:update(string.rep('\\0', 8), {area = {0, 0, 1, 1}})").find("Unknown option \"area\"."), std::string::npos);
}

// Images keep pixels on the CPU, decode and blit on the task pool, pack into atlases and become textures, and work on the pool never sees what Lua changes later.
TEST(GraphicsLuaTest, EditsImagesAndPacksThemIntoAtlases) {
    const std::vector<std::uint8_t> png = test::TestFiles::pngImage(6, 4, 0x336699FFU);
    test::LuaFixture fixture({{"content/images/tile.png", std::string(png.begin(), png.end())}});
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
    EXPECT_NE(fixture.lua("canvas:setPixel(4294967296, 0, '#000000')").find("integer out of range"), std::string::npos);
    EXPECT_EQ(fixture.lua("local copy = tile:clone() copy:fill('#000000') return assets.load('images/tile.png', 'image'):pixel(0, 0):toHex() .. ' ' .. copy:pixel(0, 0):toHex()"), "#FF336699 #FF000000");
    EXPECT_EQ(fixture.lua("tile:fill('#000000') return assets.load('images/tile.png', 'image'):pixel(0, 0):toHex()"), "#FF336699");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(canvas, {filter = 'linear', mipmaps = true}) return t.width .. ' ' .. tostring(t.mipmaps) .. ' ' .. tostring(graphics.newTexture(2, 2).mipmaps) .. ' ' .. tostring(graphics.newTexture(2, 2, {filter = 'linear'}).mipmaps) .. ' ' .. tostring(graphics.newTexture(2, 2, {filter = 'linear', mipmaps = false}).mipmaps) .. ' ' .. tostring(assets.texture('images/tile.png', {filter = 'linear'}).mipmaps)"), "16 true false true false true");
    EXPECT_EQ(fixture.lua("local builder = animation2d.newAtlasBuilder() builder:add('a', canvas) builder:add('b', tile) local atlas = builder:build({trim = false}) return atlas:frame('a').source.width .. ' ' .. #atlas:frameNames() .. ' ' .. #atlas:subset('b'):frameNames()"), "16.0 2 1");
    EXPECT_EQ(fixture.lua("canvas:bleed() return canvas:pixel(15, 0).a"), "0.0");

    EXPECT_NE(fixture.lua("graphics.newImage(0, 4)").find("An image needs a width and a height of one pixel or more."), std::string::npos);
    EXPECT_NE(fixture.lua("canvas:crop({10, 4, 8, 8})").find("is not inside the image of 16 by 8 pixels."), std::string::npos);
    EXPECT_NE(fixture.lua("canvas:blit(tile, 0, 0, {mode = 'add'})").find("Unknown option \"mode\"."), std::string::npos);
    EXPECT_NE(fixture.lua("canvas:fill('#000000', {0, 0, 1e12, 4})").find("The corner and the size of an area of an image are numbers of pixels from -1073741824 to 1073741824."), std::string::npos);

    // Areas and places far outside the image clip to nothing instead of wrapping around onto its pixels.
    EXPECT_EQ(fixture.lua("canvas:blit(tile, 2000000000, 0, {source = {-1000000000, 0, 4, 4}}) canvas:fillGradient({colors = {'#000000'}}, {-1073741824, 0, 1073741824, 4}) return canvas:pixel(13, 1):toHex()"), "#FFFF0000");
    EXPECT_NE(fixture.lua("graphics.decodeImage('not an image')").find("The image is damaged, or it is not a PNG, JPEG, TGA, BMP or GIF file."), std::string::npos);
    EXPECT_EQ(fixture.lua("return graphics.maxImageSize()"), "16384");
    EXPECT_EQ(fixture.lua("return graphics.decodeImage(png, {maxSize = 6}).width"), "6");
    EXPECT_NE(fixture.lua("graphics.decodeImage(png, {maxSize = 4})").find("pixels is too large to decode. The image may have at most 4 pixels on a side."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.decodeImageAsync(png, {maxSize = 0})").find("The option \"maxSize\" of a decode is from 1 to 16384 pixels."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.decodeImage(png, {size = 4})").find("Unknown option \"size\"."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newImage(20000, 4)").find("The image of 20000 by 4 pixels is too large. An image has at most 16384 pixels on a side."), std::string::npos);
    EXPECT_NE(fixture.lua("animation2d.newAtlasBuilder():build()").find("An atlas needs at least one image."), std::string::npos);
    EXPECT_NE(fixture.lua("local b = animation2d.newAtlasBuilder() b:add('a', canvas) b:add('a', canvas)").find("The atlas has the frame \"a\" twice."), std::string::npos);
    EXPECT_NE(fixture.lua("local b = animation2d.newAtlasBuilder() b:add('a', canvas) b:build({maxSize = 8})").find("need more room than a texture of 8 pixels."), std::string::npos);
    EXPECT_NE(fixture.lua("local b = animation2d.newAtlasBuilder() b:add('a', canvas) b:build({padding = 1, extrude = 1})").find("An atlas needs a padding of zero or more"), std::string::npos);
    EXPECT_NE(fixture.lua("local b = animation2d.newAtlasBuilder() b:add('a', canvas) b:build({rotate = true})").find("Unknown option \"rotate\"."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newTexture(canvas, {dynamic = true})").find("Unknown option \"dynamic\"."), std::string::npos);
}

// Render targets and whole frames come back as images in later frames, and images become PNG files.
TEST(GraphicsLuaTest, ReadsTargetsCapturesTheScreenAndEncodesPngFiles) {
    test::LuaFixture fixture;
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

} // namespace haylen::lua::graphics
