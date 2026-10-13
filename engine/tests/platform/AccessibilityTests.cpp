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

} // namespace haylen::platform
