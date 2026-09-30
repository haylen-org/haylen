#include <gtest/gtest.h>

#include "platform/linux/LinuxFilePattern.hpp"

namespace haylen::platform {

TEST(LinuxFilePatternTest, MatchesExtensionsInAnyCase) {
    EXPECT_EQ(LinuxFilePattern::fromExtension("png"), "*.[pP][nN][gG]");
    EXPECT_EQ(LinuxFilePattern::fromExtension("tar.gz"), "*.[tT][aA][rR].[gG][zZ]");
    EXPECT_EQ(LinuxFilePattern::fromExtension("MP3"), "*.[mM][pP]3");
    EXPECT_EQ(LinuxFilePattern::fromExtension("c++"), "*.[cC]++");
}

} // namespace haylen::platform
