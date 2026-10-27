#include <gtest/gtest.h>

#include <array>
#include <string_view>

#include "platform/SupportedOrientations.hpp"

namespace haylen::platform {

// A landscape app allows only landscape until it locks, so UIKit never turns its window to portrait for the lock screen or a snapshot, and a lock replaces the Info.plist at once.
TEST(SupportedOrientationsTest, AllowTheOrientationsOfTheInfoPlistUntilALock) {
    const std::array<std::string_view, 2> landscape{"UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight"};
    SupportedOrientations orientations;
    EXPECT_EQ(orientations.get(), SupportedOrientations::kAll);

    orientations.setListed(SupportedOrientations::fromInfoPlist(landscape));
    EXPECT_EQ(orientations.get(), SupportedOrientations::kLandscape);

    orientations.lock(Orientation::Portrait, false);
    EXPECT_EQ(orientations.get(), SupportedOrientations::kPortrait);
    orientations.lock(Orientation::Landscape, false);
    EXPECT_EQ(orientations.get(), SupportedOrientations::kLandscape);
}

TEST(SupportedOrientationsTest, ReadTheNamesOfTheInfoPlist) {
    const std::array<std::string_view, 3> names{"UIInterfaceOrientationPortrait", "UIInterfaceOrientationPortraitUpsideDown", "UIInterfaceOrientationSideways"};
    EXPECT_EQ(SupportedOrientations::fromInfoPlist(names), SupportedOrientations::kPortrait | SupportedOrientations::kPortraitUpsideDown);
    EXPECT_EQ(SupportedOrientations::fromInfoPlist({}), SupportedOrientations::kAll);
}

// Phones never turn upside down, while iPads do.
TEST(SupportedOrientationsTest, LockPhonesAndPadsApart) {
    EXPECT_EQ(SupportedOrientations::fromLock(Orientation::Portrait, false), SupportedOrientations::kPortrait);
    EXPECT_EQ(SupportedOrientations::fromLock(Orientation::Portrait, true), SupportedOrientations::kPortrait | SupportedOrientations::kPortraitUpsideDown);
    EXPECT_EQ(SupportedOrientations::fromLock(Orientation::Any, false), SupportedOrientations::kPortrait | SupportedOrientations::kLandscape);
    EXPECT_EQ(SupportedOrientations::fromLock(Orientation::Any, true), SupportedOrientations::kAll);
    EXPECT_EQ(SupportedOrientations::fromLock(Orientation::Landscape, true), SupportedOrientations::kLandscape);
}

} // namespace haylen::platform
