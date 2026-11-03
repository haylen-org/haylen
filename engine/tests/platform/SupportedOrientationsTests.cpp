#include <gtest/gtest.h>

#include "platform/SupportedOrientations.hpp"

namespace haylen::platform {

// A landscape app allows only landscape until it locks, so UIKit never turns its window to portrait for the lock screen or a snapshot, a lock replaces the orientation of the app at once, and the next app that opens its window starts again from its own.
TEST(SupportedOrientationsTest, AllowTheOrientationsOfTheAppUntilALock) {
    SupportedOrientations orientations;
    EXPECT_EQ(orientations.get(), SupportedOrientations::kAll);

    orientations.prepare(Orientation::Landscape, false);
    EXPECT_EQ(orientations.get(), SupportedOrientations::kLandscape);

    orientations.lock(Orientation::Portrait, false);
    EXPECT_EQ(orientations.get(), SupportedOrientations::kPortrait);
    orientations.lock(Orientation::Landscape, false);
    EXPECT_EQ(orientations.get(), SupportedOrientations::kLandscape);
    orientations.prepare(Orientation::Any, true);
    EXPECT_EQ(orientations.get(), SupportedOrientations::kAll);
}

TEST(SupportedOrientationsTest, LockPhonesAndPadsApart) {
    EXPECT_EQ(SupportedOrientations::fromLock(Orientation::Portrait, false), SupportedOrientations::kPortrait);
    EXPECT_EQ(SupportedOrientations::fromLock(Orientation::Portrait, true), SupportedOrientations::kPortrait | SupportedOrientations::kPortraitUpsideDown);
    EXPECT_EQ(SupportedOrientations::fromLock(Orientation::Any, false), SupportedOrientations::kPortrait | SupportedOrientations::kLandscape);
    EXPECT_EQ(SupportedOrientations::fromLock(Orientation::Any, true), SupportedOrientations::kAll);
    EXPECT_EQ(SupportedOrientations::fromLock(Orientation::Landscape, true), SupportedOrientations::kLandscape);
}

} // namespace haylen::platform
