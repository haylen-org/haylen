#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <system_error>
#include <zip.h>

#include "support/LuaFixture.hpp"

namespace haylen::lua::io {

TEST(PackageLuaTest, ListingAssetsNeverLeavesTheContentFolder) {
    test::LuaFixture fixture({{"content/maps/island.tmj", "{}"}});
    EXPECT_NE(fixture.lua("return require('haylen.assets').list('..')").find("The path \"..\" must stay inside its root folder."), std::string::npos);
}

TEST(PackageLuaTest, ReadsAssetsInRanges) {
    test::LuaFixture fixture({{"content/levels/cave.bin", "0123456789"}});
    EXPECT_EQ(fixture.lua("return require('haylen.assets').fileSize('levels/cave.bin')"), "10");
    EXPECT_EQ(fixture.lua("return require('haylen.assets').bytes('levels/cave.bin', 3, 4)"), "3456");
    EXPECT_EQ(fixture.lua("return require('haylen.assets').bytes('levels/cave.bin', 8, 100)"), "89");
    EXPECT_EQ(fixture.lua("return #require('haylen.assets').bytes('levels/cave.bin', 20, 4)"), "0");
    EXPECT_NE(fixture.lua("return require('haylen.assets').bytes('levels/cave.bin', -1, 4)").find("bad argument #2"), std::string::npos);
    EXPECT_NE(fixture.lua("return require('haylen.assets').fileSize('levels/missing.bin')").find("was not found."), std::string::npos);
}

} // namespace haylen::lua::io
