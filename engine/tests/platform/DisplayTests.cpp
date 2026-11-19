#include <gtest/gtest.h>

#include <vector>

#include "haylen/platform/Display.hpp"
#include "support/DisplayLayout.hpp"

namespace haylen::platform {

TEST(DisplayTest, FindsTheDisplayThatHoldsMostOfAFrame) {
    const std::vector<Display> displays = test::DisplayLayout::twoDisplays();
    EXPECT_EQ(Display::findHolding(displays, {-1000.0F, 400.0F, 800.0F, 600.0F}).name, "side");
    EXPECT_EQ(Display::findHolding(displays, {-300.0F, 400.0F, 800.0F, 600.0F}).name, "main");

    // A frame off every display belongs to the primary one.
    EXPECT_EQ(Display::findHolding(displays, {5000.0F, 5000.0F, 800.0F, 600.0F}).name, "main");
}

} // namespace haylen::platform
