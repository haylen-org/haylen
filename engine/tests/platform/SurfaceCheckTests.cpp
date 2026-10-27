#include <gtest/gtest.h>

#include "platform/SurfaceCheck.hpp"

namespace haylen::platform {

// The sizes of an iPhone 13 in landscape, whose screen has 844 by 390 points at a scale of 3.
TEST(SurfaceCheckTest, MatchesAViewThatCoversItsWindowWithTheDrawableOfItsBounds) {
    EXPECT_EQ(SurfaceCheck::toPixels({844.0F, 390.0F}, 3.0F), (math::Vec2{2532.0F, 1170.0F}));
    EXPECT_EQ(SurfaceCheck::toPixels({100.5F, 50.0F}, 2.0F), (math::Vec2{201.0F, 100.0F}));
    EXPECT_EQ(SurfaceCheck::toPixels({100.4F, 50.0F}, 3.0F), (math::Vec2{301.0F, 150.0F}));
    EXPECT_TRUE(SurfaceCheck::matches({.window = {844.0F, 390.0F}, .view = {844.0F, 390.0F}, .scale = 3.0F, .drawable = {2532.0F, 1170.0F}}));
}

// A view that kept the portrait frame in a landscape window drew only the left part of the screen, and a drawable left behind by its view stretches or crops the frame, so each mismatch logs once until the sizes match again.
TEST(SurfaceCheckTest, ReportsEachTimeTheViewOrTheDrawablePartFromTheWindow) {
    const SurfaceCheck::Sizes portraitView{.window = {844.0F, 390.0F}, .view = {390.0F, 844.0F}, .scale = 3.0F, .drawable = {1170.0F, 2532.0F}};
    const SurfaceCheck::Sizes staleDrawable{.window = {844.0F, 390.0F}, .view = {844.0F, 390.0F}, .scale = 3.0F, .drawable = {1170.0F, 2532.0F}};
    const SurfaceCheck::Sizes matching{.window = {844.0F, 390.0F}, .view = {844.0F, 390.0F}, .scale = 3.0F, .drawable = {2532.0F, 1170.0F}};
    EXPECT_FALSE(SurfaceCheck::matches(portraitView));
    EXPECT_FALSE(SurfaceCheck::matches(staleDrawable));

    SurfaceCheck check;
    EXPECT_FALSE(check.check(matching));
    EXPECT_TRUE(check.check(portraitView));
    EXPECT_FALSE(check.check(portraitView));
    EXPECT_FALSE(check.check(matching));
    EXPECT_TRUE(check.check(staleDrawable));
}

} // namespace haylen::platform
