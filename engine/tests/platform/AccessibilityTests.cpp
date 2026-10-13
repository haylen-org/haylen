#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/platform/Accessibility.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

TEST(AccessibilityTest, PublishesEveryChangeOfTheSettingsOnce) {
    test::EngineFixture fixture;
    std::vector<std::string> log;
    fixture.engine().getEvents().on(core::LifecycleEvent::kAccessibilitySettingsChanged, [&log](core::EventBus::Event& event) { log.push_back(event.getData().dump()); });
    Accessibility& accessibility = fixture.engine().getAccessibility();
    EXPECT_EQ(accessibility.getSettings(), AccessibilitySettings{});

    // Platform services report from their own threads, and the engine hears the last report at its next frame.
    // clang-format off
    std::thread([&fixture] {
        fixture.host().setAccessibilitySettings({.textScale = 1.5F});
        fixture.host().setAccessibilitySettings({.textScale = 1.25F, .reducedMotion = true});
    }).join();
    // clang-format on
    EXPECT_EQ(accessibility.getSettings(), AccessibilitySettings{});
    fixture.frames(2);
    EXPECT_EQ(log, (std::vector<std::string>{R"({"highContrast":false,"reducedMotion":true,"reducedTransparency":false,"screenReader":false,"textScale":1.25})"}));
    EXPECT_EQ(accessibility.getSettings(), (AccessibilitySettings{.textScale = 1.25F, .reducedMotion = true}));

    // A new app starts with what the platform reported last, without an event.
    fixture.host().setAccessibilitySettings({.highContrast = true, .reducedTransparency = true, .screenReader = true});
    fixture.restart();
    std::vector<std::string> restarted;
    fixture.engine().getEvents().on(core::LifecycleEvent::kAccessibilitySettingsChanged, [&restarted](core::EventBus::Event& event) { restarted.push_back(event.getData().dump()); });
    fixture.frames(1);
    EXPECT_TRUE(restarted.empty());
    EXPECT_TRUE(fixture.engine().getAccessibility().getSettings().screenReader);
}

TEST(AccessibilityTest, WritesElementsAsTheJsonOfTheScreenReadersOfAndroidAndTheWeb) {
    const AccessibilityElement element{.id = 4294967295U, .parent = 7, .bounds = {10.0F, 20.5F, 30.0F, 40.0F}, .role = AccessibilityElement::Role::Switch, .label = "Music", .value = "", .hint = "Plays the songs", .states = AccessibilityElement::Checked, .actions = AccessibilityElement::Focus | AccessibilityElement::Activate};
    EXPECT_EQ(element.toJson().dump(), R"({"actions":3,"bounds":[10.0,20.5,30.0,40.0],"hint":"Plays the songs","id":4294967295,"label":"Music","parent":7,"role":"switch","states":2,"value":""})");
    EXPECT_EQ(AccessibilityElement::roleFromName("radioButton"), AccessibilityElement::Role::RadioButton);
    EXPECT_FALSE(AccessibilityElement::roleFromName("robot").has_value());
}

TEST(AccessibilityLuaTest, ReadsTheSettingsAndHearsTheirChanges) {
    test::EngineFixture fixture;
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
    test::EngineFixture fixture;
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

} // namespace haylen::platform
