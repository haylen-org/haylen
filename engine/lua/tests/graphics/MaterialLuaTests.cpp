#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "haylen/core/Engine.hpp"
#include "support/AllocationTracker.hpp"
#include "support/LuaFixture.hpp"
#include "support/MaterialShaders.hpp"

namespace haylen::lua::graphics2d {

// Uniforms that Lua sets every frame, lists of numbers among them, and the draws that take the material allocate nothing once their storage has grown.
TEST(MaterialLuaTest, SetsUniformsEveryFrameWithoutAllocating) {
    const std::string compiled(test::MaterialShaders::tint().begin(), test::MaterialShaders::tint().end());
    test::LuaFixture fixture({{"content/shaders/tint.shader", compiled}});
    // clang-format off
    fixture.runLua(R"(
        graphics = require('haylen.graphics') graphics2d = require('haylen.graphics2d') scene = require('haylen.scene') m = require('haylen.math')
        tint = graphics2d.newMaterial(require('haylen.assets').shader('shaders/tint.shader'))
    )");
    // clang-format on
    // clang-format off
    const auto measure = [&fixture](const std::string& body) {
        fixture.runLua("scene.clear() scene.push({render = function() graphics2d.beginScreen() " + body + " end})");
        fixture.frames(5);
        const test::AllocationTracker tracker;
        fixture.frames(10);
        return tracker.getTotal();
    };
    // clang-format on
    const std::size_t empty = measure("");
    EXPECT_EQ(measure("tint:set('strength', 0.5) tint:set('palette', {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1}) tint:set('tint', '#FF8040FF') tint:set('offset', m.vec2(1, 2)) graphics2d.draw(graphics.whiteTexture(), 0, 0, {width = 10, height = 10, material = tint})"), empty);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(MaterialLuaTest, LoadsShadersAndDrawsWithMaterials) {
    const std::string compiled(test::MaterialShaders::tint().begin(), test::MaterialShaders::tint().end());
    test::LuaFixture fixture({{"content/shaders/tint.shader", compiled}});
    // clang-format off
    fixture.runLua(R"(
        assets = require('haylen.assets')
        graphics = require('haylen.graphics')
        graphics2d = require('haylen.graphics2d')
        m = require('haylen.math')
        shader = assets.shader('shaders/tint.shader')
        tint = graphics2d.newMaterial(shader, {tint = '#FF8040FF', strength = 0.5, offset = {1, 2}, steps = 1, transform = m.identity(), glow = {r = 1, g = 0.5, b = 0}, palette = {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1}, mask_texture = graphics.whiteTexture()})
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return shader.name .. ' ' .. #shader.uniforms .. ' ' .. shader.uniforms[1].name .. ' ' .. shader.uniforms[1].type .. ' ' .. shader.uniforms[5].count .. ' ' .. shader.textures[1]"), "tint 8 tint vec4 3 mask_texture");
    EXPECT_EQ(fixture.lua("return tostring(assets.load('shaders/tint.shader') == shader) .. ' ' .. tostring(tint.shader == shader)"), "true true");
    EXPECT_EQ(fixture.lua("return tint:get('strength') .. ' ' .. tint:get('offset').y .. ' ' .. tint:get('steps') .. ' ' .. string.format('%.2f', tint:get('tint').g) .. ' ' .. #tint:get('glow') .. ' ' .. #tint:get('palette') .. ' ' .. tint:get('mask_texture').width"), "0.5 2.0 1 0.25 3 12 1");
    EXPECT_EQ(fixture.lua("tint:set('offset', m.vec2(5, 6)) tint:set('tint', {1, 1, 1, 0.5}) tint:set('mask_texture', nil) return tint:get('offset').x .. ' ' .. tint:get('tint').a .. ' ' .. tostring(tint:get('mask_texture'))"), "5.0 0.5 nil");

    // clang-format off
    fixture.runLua(R"(
        require('haylen.scene').push({render = function()
            local camera = graphics2d.newCamera()
            graphics2d.beginWorld(camera, {ambientLight = '#FF808080', postProcess = {materials = {tint}}})
            graphics2d.draw(graphics.whiteTexture(), 0, 0, {width = 10, height = 10, material = tint})
            local sprite = graphics2d.newSprite(graphics.whiteTexture(), {material = tint})
            sprite:draw()
            spriteMaterial = sprite.material
            graphics2d.drawText(nil, 'Hi', 0, 0, {material = tint})
        end})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(spriteMaterial == tint) .. ' ' .. graphics2d.stats().canvases"), "true 1");

    // The gradient text program has no material form, so a text with both is an error.
    fixture.runLua("require('haylen.scene').replace({render = function() graphics2d.beginScreen() gradientError = select(2, pcall(graphics2d.drawText, nil, 'Hi', 0, 0, {material = tint, gradient = {colors = {'#FFFFFFFF'}}})) end})");
    fixture.frames(2);
    EXPECT_NE(fixture.lua("return gradientError").find("A text with a gradient does not take a material."), std::string::npos);

    EXPECT_NE(fixture.lua("graphics2d.newMaterial(shader, {missing = 1})").find("has no uniform named \"missing\""), std::string::npos);
    EXPECT_NE(fixture.lua("tint:set('tint', 1)").find("takes 4 numbers, not 1"), std::string::npos);
    EXPECT_NE(fixture.lua("tint:get('nothing')").find("has no uniform named \"nothing\""), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.newMaterial(shader, {[1] = 2})").find("uniform names must be strings"), std::string::npos);
    EXPECT_NE(fixture.lua("assets.shader('shaders/missing.shader')").find("error: "), std::string::npos);
}

} // namespace haylen::lua::graphics2d
