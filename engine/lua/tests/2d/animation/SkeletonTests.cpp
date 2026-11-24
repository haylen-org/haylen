#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "support/LuaFixture.hpp"
#include "support/SkeletonFiles.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::animation2d {

TEST(SkeletonLuaTest, LoadsPlaysAndDrawsSkeletonsFromLua) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(64, 96, 0xFFFFFFFFU);
    test::LuaFixture fixture({{"content/hero.json", test::SkeletonFiles::kAtlas}, {"content/hero.png", std::string(image.begin(), image.end())}, {"content/hero.skeleton", test::SkeletonFiles::kSkeleton}});
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        animation2d = require('haylen.animation2d')
        assets = require('haylen.assets')
        local data = assets.load('hero.skeleton')
        hero = animation2d.newSkeleton(data, {x = 200, y = 300, skin = 'sword'})
        heard = {}
        hero:on('event', function(event) heard[#heard + 1] = event.name .. event.track .. event.animation .. event.text .. event.number end, {owner = hero})
        hero:play('wave', {track = 1})
        hero:update(0.6)
        local bone = hero:bone('lower')
        local x, y = hero:point('tip')
        return #data.bones .. ' ' .. table.concat(data.animations, ',') .. ' ' .. hero.skin .. ' ' .. string.format('%.1f %.1f', bone.worldX, x) .. ' ' .. table.concat(heard, ',') .. ' ' .. hero:trackAnimation(1)
    )"), "5 flap,lift,wave sword 234.3 258.5 step1waveleft2.0 wave");
    // clang-format on
    EXPECT_EQ(fixture.lua("hero:crossFade('lift', 0.2, {track = 1, loop = false}) hero:setIkTarget('reach', 220, 250) hero:setIkMix('reach', 1) hero:update(0.1) local bounds = hero:bounds() return tostring(bounds.width > 0) .. ' ' .. hero.data:duration('lift')"), "true 1.0");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(hero.play, hero, 'fly') return message:match('The skeleton.*')"), "The skeleton has no animation \"fly\".");
    EXPECT_EQ(fixture.lua("local ok, message = pcall(hero.on, hero, 'step', function() end) return message:match('Unknown.*')"), "Unknown skeleton event \"step\". Skeletons report \"event\".");
}

} // namespace haylen::lua::animation2d
