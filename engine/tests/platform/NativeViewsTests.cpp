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

TEST(NativeViewsTest, KeepsTheLargestReservationOfEveryEdge) {
    NativeViews views;
    EXPECT_EQ(views.getReservedInsets(), math::Insets{});
    views.reserveInsets("banner", {.bottom = 100.0F});
    views.reserveInsets("toolbar", {.left = 30.0F, .top = 20.0F, .bottom = 60.0F});
    views.reserveInsets("rail", {.right = -40.0F});
    EXPECT_EQ(views.getReservedInsets(), (math::Insets{.left = 30.0F, .top = 20.0F, .bottom = 100.0F}));

    // A view reserves again under its key, and releasing it frees its edges.
    views.reserveInsets("banner", {.bottom = 50.0F});
    EXPECT_EQ(views.getReservedInsets().bottom, 60.0F);
    views.releaseInsets("toolbar");
    views.releaseInsets("missing");
    EXPECT_EQ(views.getReservedInsets(), (math::Insets{.bottom = 50.0F}));
}

TEST(NativeViewsTest, CountsNestedCovers) {
    NativeViews views;
    EXPECT_FALSE(views.isAppCovered());
    views.coverApp();
    views.coverApp();
    views.uncoverApp();
    EXPECT_TRUE(views.isAppCovered());
    views.uncoverApp();
    EXPECT_FALSE(views.isAppCovered());

    // An uncover without a cover is logged and never lets a later cover end early.
    views.uncoverApp();
    views.coverApp();
    EXPECT_TRUE(views.isAppCovered());
}

TEST(ReservedInsetsTest, WidensTheSafeAreaAndPublishesEveryChange) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    NativeViews& views = fixture.host().getNativeViews();
    std::vector<std::string> log;
    engine.getEvents().on(core::LifecycleEvent::kWindowSafeAreaChanged, [&log](core::EventBus::Event& event) { log.push_back(event.getData().dump()); });
    fixture.host().setSafeAreaInsets({.top = 40.0F});
    fixture.frames(1);
    log.clear();

    // Each edge takes the device inset or the largest reservation, whichever is larger.
    views.reserveInsets("banner", {.bottom = 100.0F});
    views.reserveInsets("toolbar", {.top = 20.0F, .bottom = 150.0F});
    EXPECT_EQ(engine.getReservedInsets(), math::Insets{}) << "The engine takes the reservations at the start of a frame.";
    fixture.frames(1);
    EXPECT_EQ(engine.getReservedInsets(), (math::Insets{.top = 20.0F, .bottom = 150.0F}));
    EXPECT_EQ(engine.getViewport().getSafeRect(), (math::Rect{0.0F, 40.0F, 1920.0F, 890.0F}));
    EXPECT_EQ(log, (std::vector<std::string>{R"({"height":890.0,"width":1920.0,"x":0.0,"y":40.0})"}));

    // Releasing a view gives its edge back, and a reservation inside the device inset changes nothing.
    views.releaseInsets("toolbar");
    fixture.frames(1);
    views.reserveInsets("status", {.top = 30.0F});
    fixture.frames(1);
    views.releaseInsets("banner");
    fixture.frames(1);
    EXPECT_EQ(log, (std::vector<std::string>{R"({"height":890.0,"width":1920.0,"x":0.0,"y":40.0})", R"({"height":940.0,"width":1920.0,"x":0.0,"y":40.0})", R"({"height":1040.0,"width":1920.0,"x":0.0,"y":40.0})"}));

    // A simulated safe area grows by the reservations too.
    views.reserveInsets("banner", {.bottom = 100.0F});
    engine.setSafeAreaSimulation(SafeAreaSimulation::fromJson(core::Json::array({0, 0, 10, 0})));
    fixture.frames(1);
    EXPECT_EQ(engine.getViewport().getSafeRect(), (math::Rect{0.0F, 30.0F, 1920.0F, 950.0F}));
}

TEST(ReservedInsetsTest, ReadsTheReservationsFromLuaInDesignUnits) {
    test::EngineFixture fixture({{"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "design": {"width": 960, "height": 540, "scaling": "fit"}})"}});
    fixture.host().getNativeViews().reserveInsets("banner", {.left = 20.0F, .bottom = 100.0F});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("local insets = require('haylen.viewport').reservedInsets() return table.concat({insets.left, insets.top, insets.right, insets.bottom}, ',')"), "10.0,0.0,0.0,50.0");
    EXPECT_EQ(fixture.lua("local safe = require('haylen.viewport').safeRect() return table.concat({safe.x, safe.y, safe.width, safe.height}, ',')"), "10.0,0.0,950.0,490.0");
}

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
    EXPECT_TRUE(engine.getAudio().isBusMuted("master"));
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
    EXPECT_FALSE(engine.getAudio().isBusMuted("master"));
    EXPECT_EQ(ticks, 0);
    fixture.frames(1, 0.06);
    EXPECT_EQ(ticks, 1);
    EXPECT_EQ(log, (std::vector<std::string>{"appInactive", "appActive"}));

    // The mute the player chose survives a cover.
    engine.getAudio().setBusMuted("master", true);
    views.coverApp();
    fixture.frames(1);
    views.uncoverApp();
    fixture.frames(1);
    EXPECT_TRUE(engine.getAudio().isBusMuted("master"));
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
    EXPECT_FALSE(engine.getAudio().isBusMuted("master"));
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
    EXPECT_TRUE(fixture.engine().getAudio().isBusMuted("master"));
    views.uncoverApp();
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(haylen.appCovered()) .. ' ' .. haylen.appState()"), "false active");
}

TEST(NativeViewsTest, TakesReservationsAndCoversFromOtherThreads) {
    test::EngineFixture fixture;
    NativeViews& views = fixture.host().getNativeViews();
    std::atomic<bool> done = false;
    // clang-format off
    std::thread native([&views, &done] {
        for (int index = 0; index < 300; ++index) {
            views.reserveInsets("banner", {.bottom = static_cast<float>(index % 3) * 50.0F});
            views.coverApp();
            views.uncoverApp();
        }
        views.releaseInsets("banner");
        done.store(true);
    });
    // clang-format on
    while (!done.load()) {
        fixture.frames(1);
    }
    native.join();
    fixture.frames(1);
    EXPECT_FALSE(fixture.engine().isAppCovered());
    EXPECT_EQ(fixture.engine().getReservedInsets(), math::Insets{});
    EXPECT_EQ(fixture.engine().getViewport().getSafeRect(), (math::Rect{0.0F, 0.0F, 1920.0F, 1080.0F}));
}

} // namespace haylen::platform
