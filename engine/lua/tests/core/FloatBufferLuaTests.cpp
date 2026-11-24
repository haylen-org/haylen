#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::core {

TEST(FloatBufferLuaTest, ReadsAndWritesFromLua) {
    test::LuaFixture fixture;
    fixture.runLua("buffer = require('haylen.collections').newFloatBuffer(6)");
    EXPECT_EQ(fixture.lua("buffer[1] = 2.5 buffer[6] = -1 return #buffer .. ' ' .. buffer[1] .. ' ' .. buffer[2] .. ' ' .. buffer[6]"), "6 2.5 0.0 -1.0");
    EXPECT_EQ(fixture.lua("buffer:set(2, 10, 20, 30) return table.concat({buffer:get(1, 5)}, ',')"), "2.5,10.0,20.0,30.0,0.0");
    EXPECT_EQ(fixture.lua("buffer:set(4, {7, 8, 9}) return table.concat({buffer:get(4, 3)}, ',') .. ' ' .. buffer:get(1)"), "7.0,8.0,9.0 2.5");
    EXPECT_EQ(fixture.lua("buffer:fill(1) buffer:fill(4, 2, 2) return table.concat({buffer:get(1, 6)}, ',')"), "1.0,4.0,4.0,1.0,1.0,1.0");
    EXPECT_EQ(fixture.lua("return require('haylen.collections').newFloatBuffer(2, 3)[2]"), "3.0");

    EXPECT_NE(fixture.lua("return buffer[0]").find("positions 0 to 0 fall outside its size of 6"), std::string::npos);
    EXPECT_NE(fixture.lua("return buffer:get(math.maxinteger, 2)").find("positions 9223372036854775807 to -9223372036854775808 fall outside its size of 6"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer[7] = 1").find("positions 7 to 7 fall outside"), std::string::npos);
    EXPECT_NE(fixture.lua("return buffer[1.5]").find("fall outside"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer:set(5, 1, 2, 3)").find("positions 5 to 7 fall outside"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer:set(1, {1, 'two'})").find("entry 2 of the list is not"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer[1] = 'x'").find("number expected"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer:get(1, -1)").find("the count cannot be negative"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer:fill(0, 6, 2)").find("fall outside"), std::string::npos);

    // Positions and counts past what a size of 32 bits holds fall outside too, on the web as everywhere.
    EXPECT_NE(fixture.lua("buffer:fill(1, 4294967296, 2)").find("positions 4294967296 to 4294967297 fall outside its size of 6"), std::string::npos);
    EXPECT_NE(fixture.lua("return buffer:get(1, 4294967296)").find("positions 1 to 4294967296 fall outside its size of 6"), std::string::npos);
    EXPECT_NE(fixture.lua("require('haylen.collections').newFloatBuffer(-1)").find("the size cannot be negative"), std::string::npos);
    EXPECT_NE(fixture.lua("return buffer.missing").find("has no member \"missing\""), std::string::npos);
}

} // namespace haylen::lua::core
