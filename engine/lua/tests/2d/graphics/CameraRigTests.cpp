#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::graphics2d {

TEST(CameraRigLuaTest, NamesTheZonesThatAreNotAList) {
    test::LuaFixture fixture;
    EXPECT_NE(fixture.lua("require('haylen.graphics2d').newCameraRig({zones = 1})").find("The option \"zones\" of \"newCameraRig\" is invalid: table expected, got number."), std::string::npos);
}

} // namespace haylen::lua::graphics2d
