#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

TEST(SignalFilterLuaTest, SmoothsNumbersAndVectorsFromLua) {
    test::LuaFixture fixture;
    fixture.runLua("m = require('haylen.math') average = m.movingAverage({window = 2, dimensions = 2})");
    EXPECT_EQ(fixture.lua("average:update(0, 10) local x, y = average:update(4, 20) return x .. ' ' .. y"), "2.0 15.0");
    EXPECT_EQ(fixture.lua("local point = average:update(m.vec2(8, 30)) return tostring(point.x) .. ' ' .. tostring(point.y)"), "6.0 25.0");
    EXPECT_EQ(fixture.lua("local median = m.medianFilter({window = 3}) median:update(1) median:update(90) return median:update(1)"), "1.0");
    EXPECT_EQ(fixture.lua("local hampel = m.hampelFilter() for _ = 1, 4 do hampel:update(10) end local value = hampel:update(60) return value .. ' ' .. tostring(hampel.outlier)"), "10.0 true");
    EXPECT_EQ(fixture.lua("local lowPass = m.lowPassFilter({cutoff = 2}) lowPass:update(0.1, 0) lowPass.cutoff = 3 return lowPass.cutoff .. ' ' .. lowPass.dimensions"), "3.0 1");
    EXPECT_EQ(fixture.lua("local kalman = m.kalmanFilter({processNoise = 0.1, measurementNoise = 2}) kalman:update(5) return tostring(kalman.uncertainty)"), "2.0");
    EXPECT_NE(fixture.lua("average:update(1)").find("The filter takes 2 values, not 1."), std::string::npos);
    EXPECT_NE(fixture.lua("m.medianFilter({windows = 3})").find("Unknown option \"windows\"."), std::string::npos);
    EXPECT_NE(fixture.lua("m.movingAverage({window = 0})").find("The window of a filter holds from 1 to 1024 updates, not 0."), std::string::npos);
    EXPECT_NE(fixture.lua("average:update(0 / 0, 1)").find("The values of a filter must be finite numbers."), std::string::npos);
}

} // namespace haylen::lua::math
