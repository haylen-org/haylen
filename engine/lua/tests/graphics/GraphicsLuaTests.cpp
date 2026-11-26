#include <gtest/gtest.h>

#include <lua.hpp>

#include <cstdint>
#include <filesystem>
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
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {format = 'bc7', levels = {block}, mipmaps = true})").find("The graphics API \"none\" of this device generates no mipmaps of the format \"bc7\", so a texture of it brings each of its levels."), std::string::npos);
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
    EXPECT_NE(fixture.lua("graphics.newTexture('not an image')").find("The image is damaged, or it is not a PNG, JPEG, WebP, TGA, BMP or GIF file."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newTexture(png, {fill = '#FF0000'})").find("fill"), std::string::npos);
}

// WebP images of the package load as textures and images like any other image, by their extension.
TEST(GraphicsLuaTest, LoadsWebpImagesLikeAnyImage) {
    const std::string webp = test::TestFiles::toText(test::TestFiles::read(std::filesystem::path(HAYLEN_TEST_IMAGES) / "checker-lossless.webp"));
    test::LuaFixture fixture({{"content/images/hero.webp", webp}});
    fixture.runLua("assets = require('haylen.assets') graphics = require('haylen.graphics')");
    EXPECT_EQ(fixture.lua("local texture = assets.texture('images/hero.webp') return texture.width .. 'x' .. texture.height"), "16x16");
    EXPECT_EQ(fixture.lua("return assets.load('images/hero.webp', 'image'):pixel(0, 0):toHex()"), "#FFDC3C28");
    EXPECT_EQ(fixture.lua("return graphics.decodeImage(assets.bytes('images/hero.webp')):pixel(15, 15):toHex()"), "#A0F0C83C");
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

// A texture takes a kind with its layers or slices, arrays, cubes, cube arrays and volumes keep the mipmaps that the GPU generates from their pixels, and the shape checks against the device before any pixel exists.
TEST(GraphicsLuaTest, CreatesTexturesOfEveryKind) {
    test::LuaFixture fixture;
    fixture.runLua("graphics = require('haylen.graphics')");

    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(4, 4) return t.kind .. ' ' .. t.layers .. ' ' .. t.depth .. ' ' .. t.mipLevels"), "flat 1 1 1");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(4, 4, {kind = 'array', layers = 3}) return t.kind .. ' ' .. t.layers .. ' ' .. t.depth .. ' ' .. t.mipLevels"), "array 3 1 1");
    EXPECT_EQ(fixture.lua("keptCube = graphics.newTexture(8, 8, {kind = 'cube', filter = 'linear'}) local t = keptCube return t.kind .. ' ' .. t.layers .. ' ' .. t.mipLevels .. ' ' .. tostring(t.mipmaps)"), "cube 6 4 true");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(4, 4, {kind = 'cubeArray', layers = 12}) return t.kind .. ' ' .. t.layers"), "cubeArray 12");
    EXPECT_EQ(fixture.lua("keptVolume = graphics.newTexture(4, 4, {kind = 'volume', depth = 8, filter = 'linear'}) local t = keptVolume return t.kind .. ' ' .. t.layers .. ' ' .. t.depth .. ' ' .. t.mipLevels"), "volume 1 8 4");
    EXPECT_EQ(fixture.lua("local t = graphics.newTexture(2, 2, {kind = 'array', layers = 2, pixels = string.rep(string.char(9, 8, 7, 255), 8)}) return t.layers"), "2");
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().mipmapGenerations, 2);

    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {kind = 'cube', layers = 5})").find("A texture of the kind \"cube\" has 6 layers, one for each face, not 5."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 2, {kind = 'cube'})").find("A texture of the kind \"cube\" has square faces, which 4 by 2 pixels are not."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {kind = 'cubeArray', layers = 8})").find("A texture of the kind \"cubeArray\" has 6 layers for each of its cubes, up to 2048 on this device, not 8."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {kind = 'array', layers = 4096})").find("A texture of the kind \"array\" has from 1 to 2048 layers on this device, not 4096."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {kind = 'volume', depth = 4096})").find("A texture of the kind \"volume\" has from 1 to 2048 slices on this device, not 4096."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {layers = 2})").find("A texture of the kind \"flat\" has 1 layer, not 2."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {kind = 'array', depth = 2})").find("Only a texture of the kind \"volume\" has a depth, and one of the kind \"array\" has none."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(2, 2, {kind = 'array', layers = 2, pixels = string.rep('\\0', 16)})").find("The level 1 of a texture of 2 by 2 pixels of the format \"rgba8\" holds 16 bytes, and it needs 32."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {kind = 'volume', depth = 2, format = 'bc7', levels = {string.rep('\\0', 32)}})").find("A texture of the kind \"volume\" holds no blocks of the format \"bc7\", since compressed formats hold flat blocks."), std::string::npos);
    EXPECT_NE(fixture.lua("return graphics.newTexture(4, 4, {kind = 'sphere'})").find("sphere"), std::string::npos);
    EXPECT_EQ(fixture.lua("local c = graphics.capabilities() return tostring(c.cubeArrays) .. ' ' .. tostring(c.imageViews) .. ' ' .. tostring(c.depthLayers) .. ' ' .. tostring(graphics.formatSupport('rgba8').mipmaps) .. ' ' .. tostring(graphics.formatSupport('bc7').mipmaps)"), "true true true true false");
}

// A view shows a range of the mip levels and layers of a texture as a texture of its own kind, from the whole texture by default, and refuses a range or a kind that the texture does not have.
TEST(GraphicsLuaTest, MakesViewsOfTheMipLevelsAndLayersOfTextures) {
    test::LuaFixture fixture;
    fixture.runLua("graphics = require('haylen.graphics') cube = graphics.newTexture(8, 8, {kind = 'cube', filter = 'linear'})");

    EXPECT_EQ(fixture.lua("local v = cube:view() return v.kind .. ' ' .. v.layers .. ' ' .. v.mipLevels .. ' ' .. v.width"), "cube 6 4 8");
    EXPECT_EQ(fixture.lua("local v = cube:view({kind = 'array', layer = 2, layers = 3, mipLevel = 1}) return v.kind .. ' ' .. v.layers .. ' ' .. v.mipLevels .. ' ' .. v.width"), "array 3 3 4");
    EXPECT_EQ(fixture.lua("local v = graphics.newTexture(8, 8, {kind = 'volume', depth = 8, filter = 'linear'}):view({mipLevel = 2}) return v.kind .. ' ' .. v.depth .. ' ' .. v.width"), "volume 2 2");
    EXPECT_NE(fixture.lua("return cube:view({kind = 'flat'})").find("A texture of the kind \"cube\" has no view of the kind \"flat\"."), std::string::npos);
    EXPECT_NE(fixture.lua("return cube:view({mipLevel = 2, mipLevels = 3})").find("The mip levels of a view lie within the 4 mip levels of its texture, which 3 levels from the level 2 do not."), std::string::npos);
    EXPECT_NE(fixture.lua("return cube:view({kind = 'array', layer = 4, layers = 3})").find("The layers of a view lie within the 6 layers of its texture, which 3 layers from the layer 4 do not."), std::string::npos);
    EXPECT_NE(fixture.lua("return cube:view():view()").find("A view shows a range of a texture, not of another view."), std::string::npos);
    EXPECT_NE(fixture.lua("return cube:view({slices = 2})").find("Unknown option \"slices\"."), std::string::npos);
}

// A dynamic texture of layers takes new pixels for one layer at a time, and one that keeps mipmaps has the GPU generate them after each upload.
TEST(GraphicsLuaTest, UpdatesTheLayersOfDynamicTextures) {
    test::LuaFixture fixture;
    fixture.runLua("graphics = require('haylen.graphics')");
    fixture.frames(1);
    fixture.runLua("faces = graphics.newTexture(2, 2, {kind = 'cube', dynamic = true, mipmaps = true})");
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWrites, 6);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().mipmapGenerations, 1);

    fixture.runLua("faces:update(string.rep(string.char(255, 0, 0, 255), 4), {layer = 4})");
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWrites, 1);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().imageWriteBytes, 16U);
    EXPECT_EQ(fixture.host().getGpu().getLastFrame().mipmapGenerations, 1);
    EXPECT_EQ(fixture.lua("return faces.mipLevels"), "2");
    EXPECT_NE(fixture.lua("faces:update(string.rep('\\0', 16), {layer = 6})").find("The dynamic texture has 6 layers from 0, so it has no layer 6."), std::string::npos);
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
    EXPECT_NE(fixture.lua("graphics.decodeImage('not an image')").find("The image is damaged, or it is not a PNG, JPEG, WebP, TGA, BMP or GIF file."), std::string::npos);
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

// Buffers take their usages and update by name, refuse what no device allows with a sentence, take writes and copies, and read their bytes back in a later frame, which the headless device hands over as zeros.
TEST(GraphicsLuaTest, CreatesWritesCopiesAndReadsBuffers) {
    test::LuaFixture fixture;
    fixture.runLua("graphics = require('haylen.graphics') buffer = graphics.newBuffer(16, {usage = {'vertex', 'copySource'}, update = 'dynamic', label = 'points'})");
    EXPECT_EQ(fixture.lua("return buffer.size .. ' ' .. table.concat(buffer.usage, ',') .. ' ' .. buffer.update .. ' ' .. buffer.label"), "16 vertex,copySource dynamic points");
    EXPECT_EQ(fixture.lua("local b = graphics.newBuffer('abcdefgh', {usage = {'uniform'}}) return b.size .. ' ' .. b.update"), "8 immutable");
    EXPECT_EQ(fixture.lua("local c = graphics.capabilities() return tostring(c.storageBuffers) .. ' ' .. tostring(c.indirectBuffers) .. ' ' .. c.maxUniformBlockSize .. ' ' .. c.uniformBufferAlignment"), "true true 65536 256");

    EXPECT_NE(fixture.lua("graphics.newBuffer(16, {usage = {'vertex'}})").find("An immutable buffer keeps the data it is made with"), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newBuffer(16, {usage = {}, update = 'dynamic'})").find("A buffer needs at least one usage"), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newBuffer(16, {usage = {'uniform', 'vertex'}, update = 'dynamic'})").find("A uniform buffer serves no other draw."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newBuffer(16, {usage = {'texture'}, update = 'dynamic'})").find("The buffer usage \"texture\" does not exist."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newBuffer(16, {usage = {'copySource'}, update = 'stream'})").find("A stream buffer holds new contents every frame"), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newBuffer(0, {usage = {'vertex'}, update = 'dynamic'})").find("must be a positive number of bytes."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics.newBuffer(16, {usage = {'vertex'}, update = 'dynamic', size = 4})").find("Unknown option \"size\"."), std::string::npos);
    EXPECT_NE(fixture.lua("buffer:write(2, 'abcd')").find("the offset must be a multiple of 4 bytes"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer:write(12, 'abcdefgh')").find("the range must lie inside the buffer."), std::string::npos);
    EXPECT_NE(fixture.lua("buffer:copy(buffer)").find("A copy reads a buffer with the \"copySource\" usage"), std::string::npos);

    // clang-format off
    fixture.runLua(R"(
        async = require('async')
        source = graphics.newBuffer('ABCDEFGH', {usage = {'copySource'}})
        target = graphics.newBuffer(12, {usage = {'copySource', 'copyDestination'}, update = 'dynamic'})
        buffer:write(4, 'wxyz')
        target:copy(source, {sourceOffset = 4, offset = 8, size = 4})
        async.spawn(function()
            local written = buffer:readAsync():await()
            local copied = target:readAsync(8, 4):await()
            done = #written .. ' ' .. #copied
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return done") != "nil" || fixture.engine().getError() != nullptr; }));
    ASSERT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
    EXPECT_EQ(fixture.lua("return done"), "16 4");
    EXPECT_NE(fixture.lua("buffer:readAsync(0, 6)").find("the offset must be a multiple of 4 bytes"), std::string::npos);
}

} // namespace haylen::lua::graphics
