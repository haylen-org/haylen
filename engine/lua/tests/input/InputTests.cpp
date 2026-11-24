#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::input {

TEST(InputLuaBindingTest, ReportsThePointerCapture) {
    test::LuaFixture fixture;
    fixture.runLua("input = require('haylen.input')");
    EXPECT_EQ(fixture.lua("return input.pointerCaptured()"), "false");
    fixture.engine().getInput().setPointerCaptured(true);
    EXPECT_EQ(fixture.lua("return input.pointerCaptured()"), "true");
}

TEST(InputLuaBindingTest, RejectsDeadZonesAndThresholdsOutOfRange) {
    test::LuaFixture fixture;
    fixture.runLua("input = require('haylen.input')");
    EXPECT_NE(fixture.lua("input.setGamepadDeadzone(1)").find("The gamepad dead zone must be at least 0 and below 1."), std::string::npos);
    EXPECT_NE(fixture.lua("input.setGamepadDeadzone(-0.5)").find("The gamepad dead zone must be at least 0 and below 1."), std::string::npos);
    EXPECT_NE(fixture.lua("input.setPressThreshold(0)").find("The press threshold must be above 0 and at most 1."), std::string::npos);
    EXPECT_NE(fixture.lua("input.setPressThreshold(2)").find("The press threshold must be above 0 and at most 1."), std::string::npos);
    EXPECT_EQ(fixture.lua("input.setGamepadDeadzone(0) input.setPressThreshold(1) return input.gamepadDeadzone()"), "0.0");
}

} // namespace haylen::lua::input
