#include <gtest/gtest.h>

#include <string>

#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

TEST(RaycastLuaTest, CastsRaysFromLua) {
    test::LuaFixture fixture;
    fixture.runLua("m = require('haylen.math')");

    EXPECT_EQ(fixture.lua("local hit = m.raycastSegment({0, 0}, {100, 0}, {{50, -10}, {50, 10}}) return hit.x .. ' ' .. hit.normalX .. ' ' .. hit.distance .. ' ' .. hit.fraction"), "50.0 -1.0 50.0 0.5");
    EXPECT_EQ(fixture.lua("return tostring(m.raycastSegment({0, 0}, {10, 0}, {start = {50, -10}, ['end'] = {50, 10}}))"), "nil");
    EXPECT_EQ(fixture.lua("return m.raycastRect({0, 0}, {100, 0}, m.rect(40, -5, 10, 10)).x"), "40.0");
    EXPECT_EQ(fixture.lua("return m.raycastCircle({0, 0}, {100, 0}, {center = {60, 0}, radius = 10}).distance"), "50.0");
    EXPECT_EQ(fixture.lua("return m.raycastPolygon({0, 0}, {100, 0}, {{50, -10}, {60, 0}, {50, 10}, {40, 0}}).index"), "3");
    EXPECT_EQ(fixture.lua("return m.raycastChain({30, 5}, {30, -20}, {{20, 0}, {20, 10}, {40, 10}, {40, 0}}, true).index .. ' ' .. tostring(m.raycastChain({30, 5}, {30, -20}, {{20, 0}, {20, 10}, {40, 10}, {40, 0}}))"), "4 nil");

    fixture.runLua("walls = {{{60, 0}, {60, 10}}, {{30, 0}, {30, 10}}, {{90, 0}, {90, 10}}}");
    EXPECT_EQ(fixture.lua("return m.raycastSegments({0, 5}, {100, 5}, walls).index"), "2");
    EXPECT_EQ(fixture.lua("local hits = m.raycastSegmentsAll({0, 5}, {100, 5}, walls) return #hits .. ' ' .. hits[1].index .. hits[2].index .. hits[3].index"), "3 213");
    EXPECT_EQ(fixture.lua("return #m.raycastSegmentsAll({0, 5}, {100, 5}, walls, 1)"), "1");

    fixture.runLua("mirrors = {{{100, -50}, {100, 50}}, {{0, -50}, {0, 50}}}");
    EXPECT_EQ(fixture.lua("local hits, x, y = m.bounceRay({50, 0}, {1, 0}, 1000, 2, mirrors) return #hits .. ' ' .. math.floor(hits[2].distance + 0.5) .. ' ' .. math.floor(x + 0.5)"), "3 150 100");
    EXPECT_EQ(fixture.lua("local fan = m.rayFan({50, 0}, 0, m.pi, 3, 200, mirrors) return tostring(fan[1]) .. ' ' .. fan[2].index .. ' ' .. tostring(fan[3])"), "false 1 false");
    EXPECT_NE(fixture.lua("m.bounceRay({0, 0}, {0, 0}, 10, 1, mirrors)").find("the direction must not be zero"), std::string::npos);
    EXPECT_NE(fixture.lua("m.bounceRay({0, 0}, {1, 0}, 10, -1, mirrors)").find("must not be negative"), std::string::npos);
}

} // namespace haylen::lua::math
