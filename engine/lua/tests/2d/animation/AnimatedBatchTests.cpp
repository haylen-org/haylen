#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "support/LuaFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::animation2d {

TEST(AnimatedBatchLuaTest, PlaysCrowdsFromLua) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(64, 16, 0xFFFFFFFFU);
    test::LuaFixture fixture({{"content/units.png", std::string(image.begin(), image.end())}});
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        animation2d = require('haylen.animation2d')
        assets = require('haylen.assets')
        local sheet = assets.texture('units.png')
        local walk = animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, framesPerSecond = 10})
        local die = animation2d.fromGrid(sheet, {frameWidth = 16, frameHeight = 16, cells = {3, 4}, framesPerSecond = 10, loop = 'once'})
        crowd = animation2d.newAnimatedBatch(sheet, {walk = walk, die = die})
        for index = 1, 20 do
            crowd:add({x = index * 20, y = 10, animation = index % 5 == 0 and 'die' or 'walk', speed = 1 + index % 2})
        end
        ended = {}
        crowd:on('complete', function(list) for _, index in ipairs(list) do ended[#ended + 1] = index end end, {owner = crowd})
        crowd:update(0.25)
        local sprite = crowd:get(2)
        return crowd:size() .. ' ' .. table.concat(ended, ',') .. ' ' .. sprite.animation .. ' ' .. sprite.frame .. ' ' .. sprite.source.x
    )"), "20 5,10,15,20 walk 3 32.0");
    // clang-format on
    EXPECT_EQ(fixture.lua("crowd:play(1, 'die', true) crowd:set(1, {speed = 2, scaleX = 2}) crowd:update(0.1) return crowd:animation(1) .. ' ' .. crowd:frame(1) .. ' ' .. crowd:get(1).width .. ' ' .. #crowd:finished()"), "die 2 32.0 1");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(crowd.add, crowd, {animation = 'fly'}) return message:match('The animation.*')"), "The animation \"fly\" does not exist in this animated batch.");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(crowd.on, crowd, 'loop', function() end) return message:match('Unknown.*')"), "Unknown animated batch event \"loop\". Animated batches report \"complete\".");
    EXPECT_EQ(fixture.lua("crowd:seek(3, 0.35) local sprite = crowd:get(3) return crowd:frame(3) .. ' ' .. string.format('%.2f', sprite.time) .. ' ' .. sprite.source.x"), "4 0.35 48.0");
    EXPECT_NE(fixture.lua("crowd:seek(99, 0)").find("sprite index out of range"), std::string::npos);
}

} // namespace haylen::lua::animation2d
