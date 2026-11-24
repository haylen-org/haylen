#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

TEST(MathArgumentsLuaTest, HandleNaNAndRejectInvalidRangesAndWeights) {
    test::LuaFixture fixture;
    fixture.runLua("m = require('haylen.math')");

    EXPECT_EQ(fixture.lua("return m.ease({points = {0, 1}}, 0 / 0) .. ' ' .. m.ease({points = {0, 1}}, 0.25)"), "0.0 0.25");
    EXPECT_NE(fixture.lua("return m.clamp(5, 1, 0)").find("bad argument #3 to 'clamp' (expected a maximum of at least the minimum)"), std::string::npos);
    EXPECT_NE(fixture.lua("return m.random(1):weightedIndex({1, -1})").find("Weights must be finite and not negative."), std::string::npos);
    EXPECT_NE(fixture.lua("return m.random(1):weightedIndex({1, 0 / 0})").find("Weights must be finite and not negative."), std::string::npos);
    EXPECT_NE(fixture.lua("return m.random(1):weightedIndex({0, 0})").find("A weighted pick needs at least one positive weight."), std::string::npos);
    EXPECT_NE(fixture.lua("return m.polygon.decompose({{0, 0}, {4, 0}, {4, 4}}, -1)").find("bad argument #2 to 'decompose' (expected a non-negative integer)"), std::string::npos);
}

} // namespace haylen::lua::math
