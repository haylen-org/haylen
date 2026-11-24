#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "platform/NativeViews.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

TEST(NativeViewsTest, KeepsTheLargestReservationOfEveryEdge) {
    NativeViews views;
    EXPECT_EQ(views.getReservedInsets(), haylen::math::Insets{});
    views.reserveInsets("banner", {.bottom = 100.0F});
    views.reserveInsets("toolbar", {.left = 30.0F, .top = 20.0F, .bottom = 60.0F});
    views.reserveInsets("rail", {.right = -40.0F});
    EXPECT_EQ(views.getReservedInsets(), (haylen::math::Insets{.left = 30.0F, .top = 20.0F, .bottom = 100.0F}));

    // A view reserves again under its key, and releasing it frees its edges.
    views.reserveInsets("banner", {.bottom = 50.0F});
    EXPECT_EQ(views.getReservedInsets().bottom, 60.0F);
    views.releaseInsets("toolbar");
    views.releaseInsets("missing");
    EXPECT_EQ(views.getReservedInsets(), (haylen::math::Insets{.bottom = 50.0F}));
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

TEST(NativeViewsTest, TakesReservationsAndCoversFromOtherThreads) {
    test::EngineFixture fixture;
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
    EXPECT_EQ(fixture.engine().getReservedInsets(), haylen::math::Insets{});
    EXPECT_EQ(fixture.engine().getViewport().getSafeRect(), (haylen::math::Rect{0.0F, 0.0F, 1920.0F, 1080.0F}));
}

} // namespace haylen::platform
