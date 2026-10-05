#include <gtest/gtest.h>

#include "platform/android/AndroidGlesVersion.hpp"

namespace haylen::platform {

TEST(AndroidGlesVersionTest, AsksForTheNewestVersionTheDeviceDeclares) {
    // An emulator that declares OpenGL ES 3.1 reports 3.1 in a context of 3.0 as well, so the context asks for 3.1.
    const AndroidGlesVersion emulator = AndroidGlesVersion::fromProperty("196609");
    EXPECT_EQ(emulator.major, 3);
    EXPECT_EQ(emulator.minor, 1);

    const AndroidGlesVersion newest = AndroidGlesVersion::fromProperty("196610");
    EXPECT_EQ(newest.major, 3);
    EXPECT_EQ(newest.minor, 2);
    EXPECT_EQ(AndroidGlesVersion::fromProperty("196608").minor, 0);
}

} // namespace haylen::platform
