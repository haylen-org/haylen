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
#include "support/LuaFixture.hpp"

namespace haylen::platform {

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
    test::LuaFixture fixture;
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
    test::LuaFixture fixture({{"haylen.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "design": {"width": 960, "height": 540, "scaling": "fit"}})"}});
    fixture.host().getNativeViews().reserveInsets("banner", {.left = 20.0F, .bottom = 100.0F});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("local insets = require('haylen.viewport').reservedInsets() return table.concat({insets.left, insets.top, insets.right, insets.bottom}, ',')"), "10.0,0.0,0.0,50.0");
    EXPECT_EQ(fixture.lua("local safe = require('haylen.viewport').safeRect() return table.concat({safe.x, safe.y, safe.width, safe.height}, ',')"), "10.0,0.0,950.0,490.0");
}

TEST(NativeViewsTest, TakesReservationsAndCoversFromOtherThreads) {
    test::LuaFixture fixture;
    NativeViews& views = fixture.host().getNativeViews();
    std::atomic<bool> done = false;
    // clang-format off
    std::jthread native([&views, &done] {
        for (int index = 0; index < 300; ++index) {
            views.reserveInsets("banner", {.bottom = static_cast<float>(index % 3) * 50.0F});
            views.coverApp();
            views.uncoverApp();
        }

        views.releaseInsets("banner");
        done.store(true);
    });
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&done] { return done.load(); })) << "The native thread never finished.";
    native.join();
    fixture.frames(1);
    EXPECT_FALSE(fixture.engine().isAppCovered());
    EXPECT_EQ(fixture.engine().getReservedInsets(), math::Insets{});
    EXPECT_EQ(fixture.engine().getViewport().getSafeRect(), (math::Rect{0.0F, 0.0F, 1920.0F, 1080.0F}));
}

} // namespace haylen::platform
