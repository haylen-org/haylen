#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "haylen/graphics/Device.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen {

class Graphics2DLuaTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::map<std::string, std::string> imageFiles() {
        const std::vector<std::uint8_t> image = test::TestFiles::pngImage(16, 8, 0xFFFFFFFFU);
        return {{"content/images/hero.png", std::string(image.begin(), image.end())}};
    }

    void SetUp() override {
        fixture.runLua("graphics = require('haylen.graphics') graphics2d = require('haylen.graphics2d') m = require('haylen.math') assets = require('haylen.assets') hero = assets.texture('images/hero.png')");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    // Runs the Lua body inside a scene render callback for one frame and reports the first error.
    std::string render(const std::string& body) {
        lua("require('haylen.scene').clear() renderError = nil require('haylen.scene').push({render = function() local ok, message = pcall(function() " + body + " end) if not ok then renderError = message end end})");
        fixture.frames(1);
        return lua("return tostring(renderError)");
    }

    test::EngineFixture fixture{imageFiles()};
};

TEST_F(Graphics2DLuaTest, MeasuresTextWithFonts) {
    EXPECT_EQ(lua("local font = graphics2d.defaultFont() local w, h = font:measure('Hello', {size = 32}) return w > 0 and h == font:lineHeight(32)"), "true");
    EXPECT_EQ(lua("local font = graphics2d.defaultFont() local l = font:layout('Hi\\nyou', {size = 20}) local w, h = font:measure('Hi\\nyou', {size = 20}) return #l.quads .. ' ' .. l.lineCount .. ' ' .. tostring(l.size.x == w and l.size.y == h) .. ' ' .. tostring(l.quads[1].size.x > 0 and l.quads[1].source.width > 0 and l.quads[4].position.y > l.quads[1].position.y)"), "5 2 true true");
    EXPECT_EQ(lua("local font = graphics2d.defaultFont() return font:ascent(40) > 0 and font:ascent(40) < font:lineHeight(40) and font:ascent(40) == 2 * font:ascent(20)"), "true");
    EXPECT_EQ(lua("local w, h = graphics2d.measureText(nil, 'Hello', {size = 32}) return w > 0 and h > 0"), "true");

    // The style table of `drawText` also works for `measureText`, while a font only accepts the text style.
    EXPECT_EQ(lua("local w = graphics2d.measureText(nil, 'Hello', {size = 32, layer = 2}) return w == graphics2d.measureText(nil, 'Hello', {size = 32})"), "true");

    // A scaled draw covers the block measured with the same scale.
    EXPECT_EQ(lua("local w, h = graphics2d.measureText(nil, 'Hello', {size = 32}) local sw, sh = graphics2d.measureText(nil, 'Hello', {size = 32, scale = {2, 3}}) return tostring(sw == 2 * w and sh == 3 * h)"), "true");
    EXPECT_EQ(lua("local font = graphics2d.defaultFont() local w, h = font:measure('Hello', {size = 32}) local sw, sh = font:measure('Hello', {size = 32, scale = {2, 3}}) return tostring(sw == 2 * w and sh == 3 * h)"), "true");
    EXPECT_EQ(render("graphics2d.beginScreen() graphics2d.drawText(nil, 'Hello', 0, 0, {size = 32, scale = {2, 0.5}, layer = 1})"), "nil");
    EXPECT_EQ(render("local camera = graphics2d.newCamera() camera.zoom = {1.5, 1.5} graphics2d.beginWorld(camera) graphics2d.drawText(nil, 'Small', 0.3, 0.6, {size = 9, pixelSnap = true})"), "nil");
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawText(nil, 'Hello', 0, 0, {scale = 'wide'})").find("The option \"scale\" of \"drawText\""), std::string::npos);
    EXPECT_NE(lua("graphics2d.defaultFont():measure('Hello', {layer = 2})").find("Unknown option \"layer\""), std::string::npos);
}

TEST_F(Graphics2DLuaTest, CreatesAndEditsSprites) {
    lua("sprite = graphics2d.newSprite(hero, {x = 10, y = 20, layer = 2, flipHorizontal = true, color = '#80FF0000', blend = 'additive'})");
    EXPECT_EQ(lua("return sprite.x .. ',' .. sprite.y .. ' ' .. sprite.layer .. ' ' .. tostring(sprite.flipHorizontal) .. ' ' .. tostring(sprite.flipVertical)"), "10.0,20.0 2 true false");
    EXPECT_EQ(lua("return sprite.blend .. ' ' .. sprite.color:toHex()"), "additive #80FF0000");
    EXPECT_EQ(lua("sprite.position = {5, 6} sprite.width = 32 sprite.scaleX = 2 sprite.pivotY = 1 sprite.rotation = 0.5 return sprite.position.x .. ' ' .. sprite.width .. ' ' .. sprite.scaleX .. ' ' .. sprite.pivotY"), "5.0 32.0 2.0 1.0");
    EXPECT_EQ(lua("sprite.source = {0, 0, 8, 8} sprite.flash = '#80FFFFFF' sprite.depth = 3 sprite.flipVertical = true sprite.flipHorizontal = false return sprite.source.width .. ' ' .. sprite.flash:toHex() .. ' ' .. sprite.depth .. ' ' .. tostring(sprite.flipHorizontal) .. tostring(sprite.flipVertical)"), "8.0 #80FFFFFF 3.0 falsetrue");
    EXPECT_EQ(lua("sprite.texture = graphics.whiteTexture() return sprite.texture == graphics.whiteTexture()"), "true");
    EXPECT_EQ(lua("local before = sprite.flipDiagonal sprite.flipDiagonal = true return tostring(before) .. ' ' .. tostring(sprite.flipDiagonal) .. ' ' .. tostring(graphics2d.newSprite(hero, {flipDiagonal = true}).flipDiagonal)"), "false true true");
    EXPECT_NE(lua("return graphics2d.newSprite(hero, {speed = 3})").find("Sprite\" has no writable property \"speed\""), std::string::npos);
    EXPECT_NE(lua("sprite.blend = 'burn'").find("error: "), std::string::npos);
    EXPECT_NE(lua("return graphics2d.newSprite('hero')").find("error: "), std::string::npos);
    EXPECT_EQ(lua("return graphics2d.newSprite(hero).width"), "0.0");
    EXPECT_NE(render("sprite:draw()").find("No canvas is active"), std::string::npos);
    EXPECT_EQ(render("graphics2d.beginScreen() sprite:draw()"), "nil");
    EXPECT_EQ(lua("return graphics2d.stats().sprites"), "1");
}

TEST_F(Graphics2DLuaTest, DrawsBatchesAndStaticBatches) {
    lua("batch = graphics2d.newSpriteBatch(hero) batch:reserve(8)");
    EXPECT_EQ(lua("return batch:add({x = 1, y = 2}) .. batch:add({x = 3, y = 4, color = '#FFFFFF'}) .. batch:size()"), "122");

    // A sprite without a size draws at the size of the texture, or of its source, like `graphics2d.draw`.
    EXPECT_EQ(lua("local s = batch:get(1) return s.x .. ' ' .. s.y .. ' ' .. s.width .. 'x' .. s.height .. ' ' .. s.pivotX .. ' ' .. s.color:toHex() .. ' ' .. tostring(s.flipHorizontal) .. ' ' .. tostring(s.source.width)"), "1.0 2.0 16.0x8.0 0.5 #FFFFFFFF false 0.0");
    EXPECT_EQ(lua("local index = batch:add({source = {0, 0, 4, 2}, flipVertical = true, rotation = 1, flash = '#80FFFFFF'}) local s = batch:get(index) batch:remove(index) return s.width .. 'x' .. s.height .. ' ' .. tostring(s.flipVertical) .. ' ' .. s.rotation .. ' ' .. s.flash:toHex()"), "4.0x2.0 true 1.0 #80FFFFFF");
    EXPECT_EQ(lua("local index = batch:add({flipDiagonal = true}) local diagonal = batch:get(index).flipDiagonal batch:remove(index) return tostring(diagonal) .. ' ' .. tostring(batch:get(1).flipDiagonal)"), "true false");
    EXPECT_NE(lua("batch:get(9)").find("sprite index out of range"), std::string::npos);
    EXPECT_NE(lua("batch:reserve(-1)").find("expected a non-negative integer"), std::string::npos);
    EXPECT_EQ(lua("batch:set(2, {x = 30}) batch:remove(1) return batch:size()"), "1");
    EXPECT_NE(lua("batch:set(5, {x = 1})").find("sprite index out of range"), std::string::npos);
    EXPECT_NE(lua("batch:remove(0)").find("sprite index out of range"), std::string::npos);

    EXPECT_EQ(render("graphics2d.beginWorld(graphics2d.newCamera()) baked = batch:bake() batch:draw({layer = 1}) graphics2d.drawStatic(baked) graphics2d.drawStatic(baked, 40, -8, {layer = 2})"), "nil");
    EXPECT_EQ(lua("return baked:size() .. ' ' .. baked:bounds().x .. ' ' .. tostring(baked:texture() == hero)"), "1 22.0 true");
    EXPECT_EQ(lua("return graphics2d.stats().sprites"), "3");
    EXPECT_NE(render("graphics2d.beginWorld(graphics2d.newCamera()) graphics2d.drawStatic(baked, {layer = 2})").find("number expected"), std::string::npos);
    EXPECT_EQ(lua("batch:clear() return batch:size()"), "0");

    EXPECT_EQ(render("graphics2d.beginScreen() graphics2d.drawBatch(hero, {{x = 0, y = 0}, {x = 20, y = 0, width = 4, height = 4, color = '#FF0000'}}, {layer = 2}) graphics2d.drawBatch(hero, {})"), "nil");
    EXPECT_EQ(lua("return graphics2d.stats().sprites"), "2");
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawBatch(hero, {{x = 0, size = 3}})").find("Unknown option \"size\""), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawBatch(hero, {{x = 0}}, {layr = 1})").find("Unknown option \"layr\""), std::string::npos);
}

TEST_F(Graphics2DLuaTest, ControlsCameras) {
    lua("camera = graphics2d.newCamera()");
    EXPECT_EQ(lua("return camera.viewSize.x .. 'x' .. camera.viewSize.y .. ' ' .. camera.anchor .. ' ' .. camera.zoom.x"), "1920.0x1080.0 center 1.0");
    EXPECT_EQ(lua("local center = camera:viewTransform():apply({0, 0}) return center.x .. ',' .. center.y"), "960.0,540.0");
    EXPECT_EQ(lua("camera.x = 100 camera.y = 50 return camera.position.x .. ',' .. camera.position.y"), "100.0,50.0");
    EXPECT_EQ(lua("camera.zoom = {2, 2} camera.rotation = 0 local x, y = camera:worldToScreen(100, 50) return x .. ',' .. y"), "960.0,540.0");
    EXPECT_EQ(lua("local x, y = camera:screenToWorld(960, 540) return x .. ',' .. y"), "100.0,50.0");
    EXPECT_EQ(lua("return camera:visibleBounds().width"), "960.0");

    lua("camera:snapTo(300, 400)");
    EXPECT_EQ(lua("return camera.x .. ',' .. camera.y"), "300.0,400.0");
    lua("camera.limits = {0, 0, 1000, 1000} camera:clampToLimits() camera.pixelSnap = true");
    EXPECT_EQ(lua("return camera.x .. ',' .. camera.y"), "480.0,400.0");

    // A target inside the dead zone leaves the camera in place, and one past its edge moves it by the overshoot.
    lua("camera.limits = nil camera.deadZone = {100, 60} camera:snapTo(0, 0)");
    EXPECT_EQ(lua("camera:follow(40, 25, 1) return camera.deadZone.x .. ' ' .. camera.deadZone.y .. ' ' .. camera.x .. ',' .. camera.y"), "100.0 60.0 0.0,0.0");
    EXPECT_EQ(lua("camera:follow(70, 25, 1) return camera.x .. ',' .. camera.y"), "20.0,0.0");

    // Smoothing, drag margins, look-ahead and the zoom limits are properties.
    lua("camera.positionSmoothing = true camera.positionSmoothingSpeed = 2 camera.limitSmoothing = true camera.rotationSmoothing = true camera.rotationSmoothingSpeed = 3");
    lua("camera.dragHorizontal = true camera.dragVertical = true camera.dragMargins = {0.125, 0.25, 0.5, 0.75} camera.dragOffset = {0.5, 0} camera.lookAheadTime = 0.2 camera.maxLookAhead = 50 camera.lookAheadSmoothingSpeed = 6");
    EXPECT_EQ(lua("local d = camera.dragMargins return d.left .. d.top .. d.right .. d.bottom .. ' ' .. camera.dragOffset.x .. ' ' .. camera.maxLookAhead .. ' ' .. camera.positionSmoothingSpeed .. tostring(camera.limitSmoothing)"), "0.1250.250.50.75 0.5 50.0 2.0true");
    lua("camera.maxZoom = 3 camera.minZoom = 0.5 camera.zoom = {10, 0.1}");
    EXPECT_EQ(lua("return camera.zoom.x .. ' ' .. camera.zoom.y .. ' ' .. camera.minZoom .. ' ' .. camera.maxZoom"), "3.0 0.5 0.5 3.0");
    EXPECT_NE(lua("camera.minZoom = 4").find("The smallest zoom must be above 0"), std::string::npos);
    EXPECT_NE(lua("camera.zoom = {0 / 0, 1}").find("The camera zoom must be a number."), std::string::npos);
    EXPECT_NE(lua("camera:zoomAt(0 / 0, 960, 540)").find("The camera zoom must be a number."), std::string::npos);
    lua("camera.zoom = {1, 1} camera:zoomAt(2, 960, 540) camera:align() camera:resetSmoothing()");
    EXPECT_EQ(lua("return camera.zoom.x"), "2.0");
    lua("camera:frame({{0, 0}, {100, 100}}, 10, 0.1) camera.offset = {5, 0} camera.anchor = 'topLeft' camera.ignoreRotation = true camera.viewport = {0, 0, 960, 540}");
    EXPECT_EQ(lua("return camera.offset.x .. ' ' .. camera.anchor .. ' ' .. tostring(camera.ignoreRotation) .. ' ' .. camera.viewSize.x"), "5.0 topLeft true 960.0");
    EXPECT_NE(lua("camera.anchor = 'middle'").find("unknown value 'middle'"), std::string::npos);
    EXPECT_NE(lua("camera:frame({}, 0, 1)").find("Framing needs at least one point."), std::string::npos);

    lua("camera.anchor = 'center' camera.viewport = nil camera.maxShakeOffset = 10 camera.maxShakeAngle = 0.1 camera.traumaDecay = 1 camera.shakeFrequency = 30 camera.rotationSmoothing = false camera.ignoreRotation = false camera:addTrauma(0.5)");
    EXPECT_EQ(lua("return camera.trauma .. ' ' .. tostring(camera:shakeOffset().x == 0) .. ' ' .. camera:renderRotation() .. ' ' .. camera.shakeFrequency"), "0.5 true 0.0 30.0");
    lua("camera:update(0.25)");
    EXPECT_EQ(lua("return camera.trauma"), "0.25");
    EXPECT_EQ(lua("camera.pixelSnap = false local offset = camera:shakeOffset() local position = camera:renderPosition() return tostring(offset:length() > 0) .. ' ' .. tostring((position - camera.position - camera.offset - offset):length() < 1e-4) .. ' ' .. tostring(camera:renderRotation() ~= camera.rotation)"), "true true true");
    lua("camera.trauma = 0 camera:shake(1, 1, 0) camera:update(0.1)");
    EXPECT_EQ(lua("return tostring(camera:shakeOffset().y == 0 and camera:shakeOffset().x ~= 0)"), "true");
    EXPECT_NE(lua("camera.viewSize = {1, 1}").find("no writable property \"viewSize\""), std::string::npos);

    EXPECT_EQ(lua("local a, b = graphics2d.newCamera(), graphics2d.newCamera() b.position = {100, 0} b.zoom = {4, 4} local c = graphics2d.blendCameras(a, b, 0.5) return c.x .. ' ' .. c.zoom.x"), "50.0 2.0");
    EXPECT_EQ(render("graphics2d.beginWorld(camera) camera:drawDebug({layer = 5})"), "nil");
    EXPECT_GT(std::stoi(lua("return graphics2d.stats().sprites")), 0);
}

TEST_F(Graphics2DLuaTest, KeepsCamerasInStepWithTheVisibleArea) {
    lua("camera = graphics2d.newCamera() camera.limits = {0, 0, 4000, 1000}");
    fixture.host().resize({2400.0F, 1080.0F});
    fixture.frames(1);

    // The default expand scaling widens the visible area, and a camera made before the resize sees it too.
    EXPECT_EQ(lua("return camera.viewSize.x .. 'x' .. camera.viewSize.y .. ' ' .. camera:visibleBounds().width"), "2400.0x1080.0 2400.0");
    EXPECT_EQ(lua("camera.position = {0, 500} camera:clampToLimits() return camera.x"), "1200.0");
    EXPECT_EQ(lua("camera.position = {0, 0} local x = camera:worldToScreen(0, 0) local wx = camera:screenToWorld(-240, 0) return x .. ' ' .. wx"), "960.0 -1200.0");
    EXPECT_EQ(lua("camera:follow(0, 500, 1) return camera.x"), "1200.0");
}

TEST_F(Graphics2DLuaTest, SortsMasksOffsetsAndCapturesCanvases) {
    lua("sprite = graphics2d.newSprite(hero, {sortOffset = -8, visibility = 2})");
    EXPECT_EQ(lua("return sprite.sortOffset .. ' ' .. sprite.visibility"), "-8.0 2");

    // clang-format off
    const std::string body = R"(
        local camera = graphics2d.newCamera()
        target = graphics.newRenderTarget(64, 32)
        graphics2d.beginCapture(target, '#102030')
        capturing = graphics2d.capturing()
        camera.zoom = {4, 4}
        graphics2d.beginWorld(camera, {sort = 'y', order = 2, visibilityMask = 1})
        unit = graphics2d.canvasUnitSize()
        graphics2d.draw(hero, 0, 40, {sortOffset = -30})
        graphics2d.draw(hero, 0, 20)
        sprite:draw()
        graphics2d.pushLayerOffset(3)
        graphics2d.drawRect({0, 0, 4, 4}, '#FFFFFF', {visibility = 3})
        graphics2d.popLayerOffset()
        graphics2d.endCapture()
        graphics2d.beginScreen()
        graphics2d.drawImageBlend(target.texture, graphics.whiteTexture(), {0, 0, 64, 32}, {pattern = 'radial', progress = 0.25, center = {0.25, 0.75}, reversed = true, color = '#FF0000', layer = 1})
        graphics2d.drawImageBlend(target.texture, target.texture, {0, 0, 8, 8}, {pattern = 'pixelate', blockSize = 16, cellSize = 2, angle = 1})
        graphics2d.drawImageBlend(target.texture, hero, {0, 0, 8, 8})
        after = graphics2d.capturing()
    )";
    // clang-format on
    EXPECT_EQ(render(body), "nil");
    EXPECT_EQ(lua("return tostring(capturing) .. ' ' .. tostring(after) .. ' ' .. graphics2d.stats().sprites"), "true false 3");
    EXPECT_EQ(lua("return unit"), "0.25");
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.popLayerOffset()").find("The \"popLayerOffset\" call has no matching \"pushLayerOffset\""), std::string::npos);
    EXPECT_NE(render("graphics2d.endCapture()").find("The \"endCapture\" call has no matching \"beginCapture\""), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawImageBlend(hero, hero, {0, 0, 1, 1}, {pattern = 'swirl'})").find("unknown value 'swirl'"), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawImageBlend(hero, hero, {0, 0, 1, 1}, {speed = 1})").find("Unknown option \"speed\""), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen({order = 'first'})").find("The option \"order\""), std::string::npos);
}

TEST_F(Graphics2DLuaTest, DrawsParallaxLayers) {
    lua("layer = graphics2d.newParallax(hero, {scrollScale = {0.5, 1}, repeatX = true, autoscroll = {10, 0}, color = '#80FFFFFF'}) camera = graphics2d.newCamera() camera.position = {200, 0}");
    EXPECT_EQ(lua("local x, y = layer:offset(camera) return x .. ',' .. y .. ' ' .. tostring(layer.repeatX) .. ' ' .. tostring(layer.repeatY) .. ' ' .. layer.scrollScale.x"), "100.0,0.0 true false 0.5");
    lua("layer:update(2) layer.limits = {0, 0, 100, 100} layer.position = {5, 0} layer.size = {32, 16} layer.repeatSize = {40, 16} layer.source = {0, 0, 8, 8}");
    EXPECT_EQ(lua("local x = layer:offset(camera) return x .. ' ' .. layer:scrolled().x .. ' ' .. layer.limits.width .. ' ' .. layer.size.x .. ' ' .. layer.repeatSize.x .. ' ' .. layer.source.width .. ' ' .. layer.color:toHex() .. ' ' .. layer.autoscroll.x"), "70.0 20.0 100.0 32.0 40.0 8.0 #80FFFFFF 10.0");
    EXPECT_EQ(render("graphics2d.beginWorld(camera) layer:draw(camera, {layer = -1})"), "nil");
    EXPECT_GT(std::stoi(lua("return graphics2d.stats().sprites")), 40);
    EXPECT_NE(lua("graphics2d.newParallax(hero, {speed = 1})").find("Parallax\" has no writable property \"speed\""), std::string::npos);
    EXPECT_NE(render("graphics2d.beginWorld(camera) layer.size = {0, 8} layer.repeatSize = {0, 0} layer:draw(camera)").find("A parallax layer needs a positive size to repeat."), std::string::npos);
}

TEST_F(Graphics2DLuaTest, DrawsShapesTextMeshesAndLights) {
    fixture.host().resize({960.0F, 540.0F});
    // clang-format off
    const std::string body = R"(
        local camera = graphics2d.newCamera()
        graphics2d.beginWorld(camera, {ambientLight = '#202040', sort = 'depth', postProcess = {vignetteStrength = 0.4, saturation = 0.8}})
        graphics2d.draw(hero, 10, 10, {width = 32, height = 16, scaleX = 2, rotation = 0.3, flipVertical = true, layer = 3, depth = 4, blend = 'screen', source = {0, 0, 8, 8}})
        graphics2d.drawRect({0, 0, 10, 10}, '#FF0000')
        graphics2d.drawRectOutline({0, 0, 10, 10}, 2, {1, 1, 1, 1})
        graphics2d.drawLine(0, 0, 10, 10, 2, '#00FF00', {layer = 1})
        graphics2d.drawCircle(50, 50, 10, '#0000FF', nil, 12)
        graphics2d.drawRing(50, 50, 10, 2, '#0000FF')
        graphics2d.drawArc(50, 50, 10, 2, 0, m.pi, '#FFFFFF')
        graphics2d.drawPolygon({{0, 0}, {10, 0}, {5, 8}}, '#FFFFFF')
        graphics2d.drawPolyline({{0, 0}, {10, 0}, {5, 8}}, 1, '#FFFFFF', true)
        graphics2d.drawMesh(hero, {{x = 0, y = 0, u = 0, v = 0}, {x = 10, y = 0, u = 1, v = 0}, {x = 0, y = 10, u = 0, v = 1, color = '#FF0000'}}, {1, 2, 3})
        graphics2d.drawMesh(nil, {{x = 0, y = 0}, {x = 1, y = 0}, {x = 0, y = 1}}, {1, 2, 3}, {layer = 2})
        graphics2d.drawLight({x = 10, y = 10, radius = 64, color = '#FFAA00', intensity = 0.9})
        graphics2d.drawLight({x = 10, y = 10, texture = hero, rotation = 1, scaleX = 2, scaleY = 2})
        graphics2d.pushClip({0, 0, 50, 50})
        graphics2d.drawText(nil, 'Hello', 5, 5, {size = 20, color = '#FFFFFF', align = 'center', outlineWidth = 1, maxWidth = 100})
        graphics2d.popClip()
        graphics2d.beginScreen()
        graphics2d.drawText(graphics2d.defaultFont(), 'UI', 0, 0)
        local target = graphics.newRenderTarget(32, 32)
        graphics2d.beginTarget(target, graphics2d.newCamera(), {clear = '#00000000'})
        graphics2d.drawRect({0, 0, 4, 4}, '#FFFFFF')
        bounds = graphics2d.canvasBounds()
    )";
    // clang-format on
    EXPECT_EQ(render(body), "nil");
    EXPECT_EQ(lua("local s = graphics2d.stats() return s.canvases .. ' ' .. s.lights .. ' ' .. tostring(s.drawCalls > 0 and s.vertices > 0 and s.passes >= 3)"), "3 2 true");
    EXPECT_EQ(lua("return bounds.width"), "32.0");

    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawMesh(hero, {{x = 0, y = 0}}, {0})").find("mesh indices start at 1"), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.popClip()").find("The \"popClip\" call has no matching \"pushClip\""), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen({sort = 'random'})").find("random"), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen({clear = '#000000'})").find("Screen canvases do not support a clear color."), std::string::npos);
    EXPECT_NE(render("graphics2d.beginWorld(graphics2d.newCamera(), {clear = '#000000'})").find("World canvases only support a clear color with lighting or post-processing."), std::string::npos);
    EXPECT_EQ(render("graphics2d.beginWorld(graphics2d.newCamera(), {clear = '#000000', postProcess = {}})"), "nil");
    EXPECT_NE(render("graphics2d.beginWorld(graphics2d.newCamera(), {ambientLight = '#000000'}) graphics2d.drawLight({intensity = -1})").find("A light intensity must be zero or positive."), std::string::npos);
    EXPECT_EQ(lua("return graphics2d.lightTexture().width .. ' ' .. graphics2d.lightTexture().filter .. ' ' .. tostring(graphics2d.lightTexture() ~= graphics.whiteTexture())"), "128 linear true");
    EXPECT_NE(render("graphics2d.beginScreen({ambient = '#000000'})").find("Unknown option \"ambient\""), std::string::npos);
    EXPECT_NE(render("graphics2d.beginWorld(graphics2d.newCamera(), {ambientLight = '#000000', postProcess = {vignete = 1}})").find("Unknown option \"vignete\""), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.draw(hero, 0, 0, {scale = 2})").find("Unknown option \"scale\""), std::string::npos);
    EXPECT_EQ(render("graphics2d.beginScreen() graphics2d.draw(hero, 0, 0, {flipDiagonal = true, flipHorizontal = true})"), "nil");
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawRect({0, 0, 1, 1}, '#FFFFFF', {layr = 1})").find("Unknown option \"layr\""), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawText(nil, 'x', 0, 0, {colour = '#FFFFFF'})").find("Unknown option \"colour\""), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawText(nil, 'x', 0, 0, {[1] = 'x'})").find("Option tables only accept string keys"), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawText(nil, 'x', 0, 0, {align = 'justify'})").find("justify"), std::string::npos);
}

TEST_F(Graphics2DLuaTest, BuildsNineSlices) {
    EXPECT_EQ(render("graphics2d.beginScreen() local slice = graphics2d.newNineSlice(hero, {source = {0, 0, 16, 8}, borders = {2, 2, 2, 2}, fill = 'tile'}) graphics2d.drawNineSlice(slice, {0, 0, 100, 40}, '#FFFFFF', {layer = 1}, 2)"), "nil");
    EXPECT_GT(std::stoi(lua("return graphics2d.stats().sprites")), 9);

    lua("slice = graphics2d.newNineSlice(hero, {borders = {1, 2, 3, 1}})");
    EXPECT_EQ(lua("local b = slice.borders return table.concat(b, ',') .. ' ' .. slice.fill .. ' ' .. tostring(slice.texture == hero) .. ' ' .. #slice.pieces .. ' ' .. slice.pieces[5].width .. ' ' .. tostring(slice.valid)"), "1.0,2.0,3.0,1.0 stretch true 9 12.0 true");
    lua("slice.fill = 'tile' slice.texture = graphics.whiteTexture() local pieces = slice.pieces pieces[1] = {0, 0, 5, 5} slice.pieces = pieces");
    EXPECT_EQ(lua("return slice.fill .. ' ' .. tostring(slice.texture == graphics.whiteTexture()) .. ' ' .. slice.borders[1] .. ' ' .. slice.pieces[1].width"), "tile true 5.0 5.0");
    EXPECT_NE(lua("slice.pieces = {{0, 0, 1, 1}}").find("a nine-slice needs exactly nine pieces"), std::string::npos);
    EXPECT_NE(lua("slice.fill = 'mirror'").find("unknown value 'mirror'"), std::string::npos);
    EXPECT_NE(lua("slice.borders = {}").find("no writable property \"borders\""), std::string::npos);

    lua("pieces = {} for i = 1, 9 do pieces[i] = {0, 0, 2, 2} end");
    EXPECT_EQ(render("graphics2d.beginScreen() graphics2d.drawNineSlice(graphics2d.newNineSlice(hero, {pieces = pieces}), {0, 0, 10, 10})"), "nil");
    EXPECT_NE(lua("return graphics2d.newNineSlice(hero, {pieces = {{0, 0, 1, 1}}})").find("a nine-slice needs exactly nine pieces"), std::string::npos);
    EXPECT_NE(lua("return graphics2d.newNineSlice(hero, {borders = {1, 2}})").find("borders need left, top, right and bottom"), std::string::npos);
    EXPECT_NE(lua("return graphics2d.newNineSlice(hero, {borders = {1, 1, 1, 1}, fill = 'mirror'})").find("unknown value 'mirror'"), std::string::npos);
    EXPECT_NE(lua("return graphics2d.newNineSlice(hero, {source = {0, 0, 8, 8}, borders = {5, 1, 5, 1}})").find("The borders of a nine-slice must fit inside its source rectangle"), std::string::npos);

    lua("frame = graphics2d.newNineSlice(hero, {source = {0, 0, 12, 12}, borders = {4, 4, 4, 4}})");
    EXPECT_EQ(lua("local patches = frame:layout({10, 20, 40, 30}, 2) local center = patches[5] return #patches .. ' ' .. center.area.x .. ' ' .. center.area.width .. ' ' .. center.source.x .. ' ' .. center.source.width"), "9 18.0 24.0 4.0 4.0");
    EXPECT_EQ(lua("return #frame:layout({0, 0, 0, 30})"), "0");
    EXPECT_NE(lua("return frame:layout({0, 0, 10, 10}, -1)").find("A nine-slice border scale must be positive."), std::string::npos);
}

TEST_F(Graphics2DLuaTest, LightsShadowsNormalMapsAndMetaballs) {
    // clang-format off
    const std::string body = R"(
        local lighting2d = require('haylen.lighting2d')
        local lamp = lighting2d.newLight({x = 20, y = 20, radius = 120, intensity = 2, shadows = true, shadowFilter = 'pcf13'})
        graphics2d.beginWorld(graphics2d.newCamera(), {ambientLight = '#FF101010'})
        graphics2d.draw(hero, 0, 0, {normalMap = hero, specular = 0.5, shininess = 40, lightMask = 3, layer = 2})
        graphics2d.drawRect({0, 0, 10, 10}, '#FF0000', {unshaded = true})
        graphics2d.drawText(nil, 'Glow', 0, 0, {emission = 1.5})
        graphics2d.drawOccluder(lighting2d.newOccluder({points = {40, 0, 40, 40, 50, 40}}))
        graphics2d.drawOccluder({points = {{0, 60}, {30, 60}}, closed = false, cull = 'clockwise'})
        graphics2d.drawLight(lamp)
        graphics2d.drawLight({type = 'directional', rotation = 1, blend = 'mix', layerMin = 1})
        graphics2d.drawMetaballs({0, 0, 10, 0, 20, 5}, 8, {color = '#FF2080FF', outlineColor = '#FFFFFFFF', outlineWidth = 0.1, threshold = 0.4, layer = 1})
        local sprite = graphics2d.newSprite(hero, {normalMap = hero, specular = 0.25, emission = 0.5, lightMask = 2, unshaded = true, shininess = 20})
        sprite:draw()
        spriteState = tostring(sprite.normalMap == hero) .. ' ' .. sprite.specular .. ' ' .. sprite.lightMask .. ' ' .. tostring(sprite.unshaded) .. ' ' .. sprite.shininess
        sprite.normalMap = nil
        spriteState = spriteState .. ' ' .. tostring(sprite.normalMap) .. ' ' .. tostring(sprite.material)
    )";
    // clang-format on
    EXPECT_EQ(render(body), "nil");
    EXPECT_EQ(lua("local s = graphics2d.stats() return s.lights .. ' ' .. s.occluders .. ' ' .. s.shadows .. ' ' .. tostring(graphics2d.hdrLighting())"), "2 2 1 true");
    EXPECT_EQ(lua("return spriteState"), "true 0.25 2 true 20.0 nil nil");

    EXPECT_NE(render("graphics2d.beginWorld(graphics2d.newCamera()) graphics2d.drawOccluder({points = {0, 0, 1, 1}, closed = false})").find("Occluders can only be drawn in a lit canvas"), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawMetaballs({0, 0, 1}, 4)").find("two numbers per point"), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawMetaballs({0, 0}, 4, {threshold = 2})").find("A metaball threshold must be between 0 and 1."), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.drawMetaballs({0, 0}, 4, {size = 2})").find("Unknown option \"size\""), std::string::npos);
    EXPECT_NE(render("graphics2d.beginScreen() graphics2d.draw(hero, 0, 0, {lightMask = 256})").find("integer out of range"), std::string::npos);
    EXPECT_NE(render("graphics2d.beginWorld(graphics2d.newCamera(), {postProcess = {materials = {1}}})").find("haylen.Material expected"), std::string::npos);
}

// Frames that light, shadow, post-process, capture, blend, write changing text and UI and run scene transitions reuse or release every GPU object they need.
TEST_F(Graphics2DLuaTest, KeepsGpuPoolsSteadyAcrossFrames) {
    // clang-format off
    lua(R"(
        local lighting2d = require('haylen.lighting2d')
        local scene = require('haylen.scene')
        local ui = require('haylen.ui')
        local lamp = lighting2d.newLight({x = 20, y = 20, radius = 120, shadows = true})
        local occluder = lighting2d.newOccluder({points = {40, 0, 40, 40, 50, 40}})
        local capture = graphics.newRenderTarget(64, 32)
        local target = graphics.newRenderTarget(32, 32)
        local rich = graphics2d.newRichText('[color=#FF0000]Hello[/color] world', {size = 16})
        local camera = graphics2d.newCamera()
        local hud = ui.mount(ui.column{ui.label{id = 'frame', text = ''}, ui.button{text = 'Play'}})
        local frame = 0
        local stages = {}
        for index = 1, 2 do
            stages[index] = {
                update = function()
                    frame = frame + 1
                    hud:set('frame', {text = 'Frame ' .. frame})
                    if frame % 40 == 0 then
                        scene.replace(stages[3 - index], {duration = 0.1, effect = index == 1 and 'pageTurn' or 'crossFade'})
                    end
                end,
                render = function()
                    graphics2d.beginCapture(capture, '#000000')
                    graphics2d.beginWorld(camera, {ambientLight = '#202040', postProcess = {vignetteStrength = 0.4}})
                    graphics2d.draw(hero, 0, 0, {normalMap = hero})
                    graphics2d.drawOccluder(occluder)
                    graphics2d.drawLight(lamp)
                    graphics2d.drawText(nil, 'Frame ' .. frame, 0, 0, {size = 12 + frame % 5})
                    graphics2d.drawMetaballs({0, 0, 10, 0}, 8)
                    graphics2d.endCapture()
                    graphics2d.beginTarget(target, camera, {clear = '#00000000'})
                    graphics2d.drawRect({0, 0, 4, 4}, '#FFFFFF')
                    graphics2d.beginScreen()
                    rich:draw(0, 0)
                    graphics2d.drawImageBlend(capture.texture, target.texture, {0, 0, 64, 32}, {pattern = 'radial', progress = 0.5})
                end,
            }
        end
        scene.clear()
        scene.push(stages[1])
    )");
    // clang-format on

    // Both measures fall at the same point between two transitions, after every transition kind ran once.
    fixture.frames(90);
    const std::vector<graphics::Device::Pool> warm = fixture.engine().getGraphics().getPools();
    fixture.frames(240);
    const std::vector<graphics::Device::Pool> later = fixture.engine().getGraphics().getPools();
    ASSERT_EQ(fixture.engine().getError(), nullptr);
    ASSERT_EQ(warm.size(), later.size());
    for (std::size_t index = 0; index < warm.size(); ++index) {
        EXPECT_EQ(warm[index].used, later[index].used) << warm[index].name;
    }
}

} // namespace haylen
