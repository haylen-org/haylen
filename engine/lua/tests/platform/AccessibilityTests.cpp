#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::platform {

TEST(AccessibilityLuaTest, ReadsTheSettingsAndHearsTheirChanges) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        accessibility = require('haylen.accessibility')
        events = require('haylen.events')
        heard = {}
        events.on('accessibilitySettingsChanged', function(settings) heard[#heard + 1] = settings.textScale .. ' ' .. tostring(settings.highContrast) end)
        describe = function()
            local settings = accessibility.settings()
            return table.concat({settings.textScale, tostring(settings.reducedMotion), tostring(settings.highContrast), tostring(settings.reducedTransparency), tostring(settings.screenReader)}, ' ')
        end
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return describe()"), "1.0 false false false false");

    fixture.host().setAccessibilitySettings({.textScale = 2.0F, .reducedMotion = true, .highContrast = true, .reducedTransparency = true, .screenReader = true});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return describe()"), "2.0 true true true true");
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "2.0 true");
}

TEST(AccessibilityLuaTest, ShortensAndSkipsTheMotionThatOptsIn) {
    test::LuaFixture fixture;
    fixture.host().setAccessibilitySettings({.reducedMotion = true});
    fixture.frames(1);
    // clang-format off
    fixture.runLua(R"(
        tween = require('haylen.tween')
        scene = require('haylen.scene')
        panel = {x = 0, y = 0}
        tween.to(panel, 1, {x = 100}, {reducedMotion = 'skip'})
        tween.to(panel, 1, {y = 100}, {reducedMotion = 'shorten'})
        scene.push({enter = function() entered = true end}, {duration = 2, effect = 'fade', reducedMotion = 'skip'})
    )");
    // clang-format on
    fixture.frames(1, 0.125);
    EXPECT_EQ(fixture.lua("return panel.x .. ' ' .. panel.y .. ' ' .. tostring(entered)"), "100.0 50.0 true");
    EXPECT_NE(fixture.lua("tween.to(panel, 1, {x = 0}, {reducedMotion = 'slow'})").find("slow"), std::string::npos);
}

} // namespace haylen::lua::platform
