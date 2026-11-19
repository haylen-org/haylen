#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/TimerScheduler.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/SafeAreaSimulation.hpp"
#include "platform/NativeViews.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

class AppCoverTest : public ::testing::Test {
  protected:
    // Records every app state event of the engine by name.
    static void recordStates(core::Engine& engine, std::vector<std::string>& log) {
        for (const std::string_view name : {core::LifecycleEvent::kAppActive, core::LifecycleEvent::kAppInactive, core::LifecycleEvent::kAppBackground}) {
            engine.getEvents().on(name, [&log, name](core::EventBus::Event&) { log.emplace_back(name); });
        }
    }
};

TEST_F(AppCoverTest, MakesTheAppInactiveHaltedAndMutedWhileCovered) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    NativeViews& views = fixture.host().getNativeViews();
    std::vector<std::string> log;
    recordStates(engine, log);
    int ticks = 0;
    engine.getTimers().every(0.05F, [&ticks] { ++ticks; });

    // Covers nest, and the app stays covered until the last one ends, whatever the lifecycle options say.
    views.coverApp();
    views.coverApp();
    fixture.frames(3, 0.06);
    EXPECT_TRUE(engine.isAppCovered());
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Inactive);
    EXPECT_TRUE(engine.isHalted());
    EXPECT_TRUE(engine.getAudio().isOutputMuted());
    EXPECT_EQ(ticks, 0);
    views.uncoverApp();
    fixture.frames(2, 0.06);
    EXPECT_TRUE(engine.isAppCovered());
    EXPECT_EQ(ticks, 0);

    // The last uncover brings the app back without a jump of the clock.
    views.uncoverApp();
    fixture.frames(1, 10.0);
    EXPECT_FALSE(engine.isAppCovered());
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Active);
    EXPECT_FALSE(engine.isHalted());
    EXPECT_FALSE(engine.getAudio().isOutputMuted());
    EXPECT_EQ(ticks, 0);
    fixture.frames(1, 0.06);
    EXPECT_EQ(ticks, 1);
    EXPECT_EQ(log, (std::vector<std::string>{"appInactive", "appActive"}));

    // The mute the player chose survives a cover, which keeps the app silent whatever the app unmutes meanwhile, and the app reads its own mute all along.
    engine.getAudio().setBusMuted("master", true);
    views.coverApp();
    fixture.frames(1);
    EXPECT_TRUE(engine.getAudio().isOutputMuted());
    engine.getAudio().setBusMuted("master", false);
    EXPECT_FALSE(engine.getAudio().isBusMuted("master"));
    EXPECT_TRUE(engine.getAudio().isOutputMuted());
    engine.getAudio().setBusMuted("master", true);
    views.uncoverApp();
    fixture.frames(1);
    EXPECT_TRUE(engine.getAudio().isBusMuted("master"));
    EXPECT_FALSE(engine.getAudio().isOutputMuted());
}

TEST_F(AppCoverTest, DrawsTheCoveredFrameOnce) {
    test::EngineFixture fixture({{"source/main.lua", "renders = 0 require('haylen.scene').push({render = function() renders = renders + 1 end})"}});
    core::Engine& engine = fixture.engine();
    NativeViews& views = fixture.host().getNativeViews();
    const auto rendered = [&fixture] { return std::stoi(fixture.lua("return renders")); };
    fixture.frames(2);
    int before = rendered();

    // The first covered frame shows the app covered and stays on screen, since a halted app would draw it the same again.
    views.coverApp();
    fixture.frames(4);
    EXPECT_EQ(rendered() - before, 1);

    // A window of another size and a surface that the background may have dropped need the frame once more.
    engine.handleEvent({.type = Event::Type::Resized});
    fixture.frames(3);
    EXPECT_EQ(rendered() - before, 2);
    engine.handleEvent({.type = Event::Type::Suspended});
    fixture.frames(2);
    engine.handleEvent({.type = Event::Type::Resumed});
    fixture.frames(3);
    EXPECT_EQ(rendered() - before, 3);

    // The app draws every frame again once the cover ends.
    views.uncoverApp();
    before = rendered();
    fixture.frames(3);
    EXPECT_EQ(rendered() - before, 3);
}

TEST_F(AppCoverTest, KeepsTheAppCoveredThroughTheBackground) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    NativeViews& views = fixture.host().getNativeViews();
    std::vector<std::string> log;
    recordStates(engine, log);

    // A covered app that comes back from the background, or gets the focus back, stays inactive until the cover ends.
    views.coverApp();
    fixture.frames(1);
    engine.handleEvent({.type = Event::Type::Suspended});
    EXPECT_TRUE(engine.isHalted());
    engine.handleEvent({.type = Event::Type::Resumed});
    engine.handleEvent({.type = Event::Type::FocusLost});
    engine.handleEvent({.type = Event::Type::FocusGained});
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Inactive);
    views.uncoverApp();
    fixture.frames(1);
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Active);

    // A cover that ends in the background leaves the app there until the platform brings it back.
    views.coverApp();
    fixture.frames(1);
    engine.handleEvent({.type = Event::Type::Suspended});
    views.uncoverApp();
    fixture.frames(1);
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Background);
    EXPECT_FALSE(engine.isAppCovered());
    engine.handleEvent({.type = Event::Type::Resumed});
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Active);
    EXPECT_FALSE(engine.getAudio().isOutputMuted());
    EXPECT_EQ(log, (std::vector<std::string>{"appInactive", "appBackground", "appInactive", "appActive", "appInactive", "appBackground", "appActive"}));
}

TEST_F(AppCoverTest, CoversAnAppThatRestartsUnderNativeUi) {
    test::EngineFixture fixture({{"source/main.lua", "haylen = require('haylen') events = require('haylen.events') states = {} events.on('appInactive', function() states[#states + 1] = haylen.appCovered() end)"}});
    NativeViews& views = fixture.host().getNativeViews();
    views.coverApp();
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat({tostring(haylen.appCovered()), haylen.appState(), tostring(haylen.halted()), tostring(states[1])}, ' ')"), "true inactive true true");

    // The cover belongs to the platform, so the restarted app hears it on its first frame.
    fixture.restart();
    EXPECT_FALSE(fixture.engine().isAppCovered());
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat({tostring(haylen.appCovered()), haylen.appState(), #states}, ' ')"), "true inactive 1");
    EXPECT_TRUE(fixture.engine().getAudio().isOutputMuted());
    views.uncoverApp();
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(haylen.appCovered()) .. ' ' .. haylen.appState()"), "false active");
}

} // namespace haylen::platform
