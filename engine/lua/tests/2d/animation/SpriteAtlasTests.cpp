#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "support/LuaFixture.hpp"
#include "support/SpriteAtlasFiles.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::animation2d {

TEST(Animation2DLuaTest, AnimatesSpritesFromLua) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(64, 32, 0xFFFFFFFFU);
    test::LuaFixture fixture({{"content/ui/hero.json", test::SpriteAtlasFiles::kTaggedAtlas}, {"content/ui/hero.png", std::string(image.begin(), image.end())}, {"content/units/warrior.png", std::string(image.begin(), image.end())}});
    // clang-format off
    fixture.runLua(R"(
        animation2d = require('haylen.animation2d')
        graphics2d = require('haylen.graphics2d')
        assets = require('haylen.assets')
        sheet = assets.texture('units/warrior.png')
        run = animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, framesPerSecond = 10})
        slash = animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, cells = {2, 3}, framesPerSecond = 5, loop = 'once', margin = {0, 0}, spacing = {0, 0}})
        animator = animation2d.newAnimator()
        animator:add('run', run)
        animator:add('slash', slash)
        events = {}
        animator.onFrame = function(name, frame) events[#events + 1] = name .. frame end
        animator.onComplete = function(name) events[#events + 1] = name .. ' done' animator:play('run') end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("return run.frameCount .. ' ' .. string.format('%.2f', run.duration) .. ' ' .. run.loop .. ' ' .. tostring(run.texture == sheet)"), "8 0.80 loop true");
    EXPECT_EQ(fixture.lua("local r = slash:frame(1) return r.x .. ' ' .. slash:frameAt(1)"), "16.0 2");
    EXPECT_EQ(fixture.lua("animator:play('slash') animator:update(0.25) animator:update(0.3) return table.concat(events, ',')"), "slash2,slash done");
    EXPECT_EQ(fixture.lua("return animator.current .. ' ' .. animator.frame .. ' ' .. tostring(animator.playing) .. ' ' .. tostring(animator.finished) .. ' ' .. animator.time"), "run 1 true false 0.0");
    EXPECT_EQ(fixture.lua("local sprite = graphics2d.newSprite(sheet) animator.speed = 2 animator.pivotY = 1 animator:update(0.1) animator:apply(sprite) return sprite.source.x .. ' ' .. sprite.pivotY"), "32.0 1.0");
    EXPECT_EQ(fixture.lua("return type(animator.onFrame) .. ' ' .. tostring(animator:has('run'))"), "function true");
    EXPECT_EQ(fixture.lua("animator.onFrame = nil animator:stop() return tostring(animator.onFrame) .. ' ' .. tostring(animator.playing)"), "nil false");
    EXPECT_EQ(fixture.lua("return animation2d.newAnimator().current"), "nil");
    EXPECT_EQ(fixture.lua("animator.onComplete = nil animator.speed = 1 animator:play('slash') animator:queue('run') local before = animator.queuedCount animator:update(0.5) return before .. ' ' .. animator.queuedCount .. ' ' .. animator.current"), "1 0 run");
    EXPECT_EQ(fixture.lua("animator:queue('slash') animator:clearQueue() return animator.queuedCount"), "0");
    EXPECT_EQ(fixture.lua("animator:play('slash', true) animator:update(5) animator:queue('slash') return animator.frame .. ' ' .. tostring(animator.finished) .. ' ' .. tostring(animator.playing)"), "1 false true");
    EXPECT_EQ(fixture.lua("local a = animator:animation('slash') return a.frameCount .. ' ' .. a.loop .. ' ' .. string.format('%.2f %.2f', run.cycleDuration, animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, loop = 'pingPong'}).cycleDuration)"), "2 once 0.80 1.40");
    EXPECT_NE(fixture.lua("animator:animation('fly')").find("The animation \"fly\" does not exist in this animator."), std::string::npos);

    fixture.runLua("atlas = assets.load('ui/hero.json', 'atlas')");
    EXPECT_EQ(fixture.lua("return #atlas:frameNames() .. ' ' .. table.concat(atlas:animationNames(), ',') .. ' ' .. table.concat(atlas:sliceNames(), ',')"), "4 back,bounce,hit,walk panel");
    EXPECT_EQ(fixture.lua("return atlas:animation('walk').frameCount .. ' ' .. atlas:source('hero 1').x .. ' ' .. tostring(atlas.texture == assets.texture('ui/hero.png'))"), "3 8.0 true");
    EXPECT_EQ(fixture.lua("return tostring(atlas == assets.load('ui/hero.json', 'atlas')) .. ' ' .. tostring(atlas == assets.load('ui/hero.json', 'atlas', {filter = 'linear'})) .. ' ' .. tostring(atlas == atlas.texture)"), "true false false");
    EXPECT_EQ(fixture.lua("local s = graphics2d.newSprite(atlas.texture) atlas:apply(s, 'hero 0') return s.width .. ' ' .. s.pivotX"), "8.0 0.5");
    EXPECT_EQ(fixture.lua("return atlas:animationFromFrames({'hero 1', 'hero 2'}, {framesPerSecond = 2, loop = 'pingPong'}).duration"), "1.0");
    EXPECT_EQ(fixture.lua("return type(atlas:slice('panel'))"), "userdata");
    EXPECT_EQ(fixture.lua("return tostring(atlas:hasFrame('hero 0')) .. tostring(atlas:hasFrame('nope')) .. tostring(atlas:hasAnimation('walk')) .. tostring(atlas:hasAnimation('fly')) .. tostring(atlas:hasSlice('panel')) .. tostring(atlas:hasSlice('marker'))"), "truefalsetruefalsetruefalse");
    EXPECT_EQ(fixture.lua("local f = atlas:frame('hero 0') return f.source.width .. ' ' .. f.offset.x .. ' ' .. f.originalSize.x .. ' ' .. string.format('%.2f %.2f', f.duration, atlas:frame('hero 1').duration)"), "8.0 4.0 16.0 0.10 0.20");
    EXPECT_EQ(fixture.lua("return animation2d.fromFrames(sheet, {{0, 0, 8, 8}, {8, 0, 8, 8}}, {framesPerSecond = 4}).duration"), "0.5");

    EXPECT_NE(fixture.lua("return animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, speed = 2})").find("Unknown option \"speed\""), std::string::npos);
    EXPECT_NE(fixture.lua("return animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, framesPerSecond = 0})").find("framesPerSecond must be positive"), std::string::npos);
    EXPECT_NE(fixture.lua("return animation2d.fromFrames(sheet, {})").find("expected at least one frame"), std::string::npos);
    EXPECT_NE(fixture.lua("return run:frame(9)").find("frame index out of range"), std::string::npos);
    EXPECT_NE(fixture.lua("animator:play('fly')").find("The animation \"fly\" does not exist in this animator."), std::string::npos);
    EXPECT_NE(fixture.lua("animator.onComplete = 3").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("return atlas:source('nope')").find("The frame \"nope\" does not exist in this atlas."), std::string::npos);

    fixture.runLua("animator:play('slash', true) animator.onComplete = function() error('finish failed') end");
    EXPECT_NE(fixture.lua("animator:update(5)").find("finish failed"), std::string::npos);
}

TEST(Animation2DLuaTest, SeeksTheCurrentAnimationWithoutCallbacks) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(64, 32, 0xFFFFFFFFU);
    test::LuaFixture fixture({{"content/units/warrior.png", std::string(image.begin(), image.end())}});
    // clang-format off
    fixture.runLua(R"(
        animation2d = require('haylen.animation2d')
        graphics2d = require('haylen.graphics2d')
        sheet = require('haylen.assets').texture('units/warrior.png')
        animator = animation2d.newAnimator()
        animator:add('run', animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, framesPerSecond = 10}))
        reports = 0
        animator.onFrame = function() reports = reports + 1 end
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("animator:seek(0.5) return tostring(animator.current) .. ' ' .. animator.time"), "nil 0.0");
    EXPECT_EQ(fixture.lua("animator:play('run') animator:seek(0.35) local sprite = graphics2d.newSprite(sheet) animator:apply(sprite) return animator.frame .. ' ' .. string.format('%.2f', animator.time) .. ' ' .. sprite.source.x .. ' ' .. reports"), "4 0.35 48.0 0");
    EXPECT_EQ(fixture.lua("animator:seek(0.65) return animator.frame .. ' ' .. reports"), "7 0");
    EXPECT_NE(fixture.lua("animator:seek('soon')").find("number expected"), std::string::npos);
}

} // namespace haylen::lua::animation2d
