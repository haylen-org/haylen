#include <gtest/gtest.h>

#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "haylen/math/Ray.hpp"
#include "haylen/math/Raycast.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

TEST(RaycastTest, HitsSegmentsRectsAndCirclesWithFacingNormals) {
    const haylen::math::Ray ray = haylen::math::Ray::between({0.0F, 0.0F}, {100.0F, 0.0F});

    const std::optional<haylen::math::RayHit> wall = haylen::math::Raycast::segment(ray, {{50.0F, -10.0F}, {50.0F, 10.0F}});
    ASSERT_TRUE(wall.has_value());
    EXPECT_FLOAT_EQ(wall->distance, 50.0F);
    EXPECT_EQ(wall->normal, (haylen::math::Vec2{-1.0F, 0.0F}));
    EXPECT_FALSE(haylen::math::Raycast::segment(ray, {{150.0F, -10.0F}, {150.0F, 10.0F}}).has_value());
    EXPECT_FALSE(haylen::math::Raycast::segment(ray, {{10.0F, 0.0F}, {20.0F, 0.0F}}).has_value());

    const std::optional<haylen::math::RayHit> box = haylen::math::Raycast::rect(ray, {40.0F, -5.0F, 10.0F, 10.0F});
    ASSERT_TRUE(box.has_value());
    EXPECT_EQ(box->point, (haylen::math::Vec2{40.0F, 0.0F}));
    EXPECT_EQ(box->normal, (haylen::math::Vec2{-1.0F, 0.0F}));
    const std::optional<haylen::math::RayHit> fromBelow = haylen::math::Raycast::rect(haylen::math::Ray::between({45.0F, 50.0F}, {45.0F, 0.0F}), {40.0F, -5.0F, 10.0F, 10.0F});
    ASSERT_TRUE(fromBelow.has_value());
    EXPECT_EQ(fromBelow->normal, (haylen::math::Vec2{0.0F, 1.0F}));
    EXPECT_FLOAT_EQ(fromBelow->distance, 45.0F);
    EXPECT_FALSE(haylen::math::Raycast::rect(ray, {40.0F, 5.0F, 10.0F, 10.0F}).has_value());

    const std::optional<haylen::math::RayHit> ball = haylen::math::Raycast::circle(ray, {{60.0F, 0.0F}, 10.0F});
    ASSERT_TRUE(ball.has_value());
    EXPECT_FLOAT_EQ(ball->distance, 50.0F);
    EXPECT_EQ(ball->normal, (haylen::math::Vec2{-1.0F, 0.0F}));
    EXPECT_FALSE(haylen::math::Raycast::circle(ray, {{-60.0F, 0.0F}, 10.0F}).has_value());
    EXPECT_FALSE(haylen::math::Raycast::circle(ray, {{60.0F, 20.0F}, 10.0F}).has_value());
    EXPECT_FALSE(haylen::math::Raycast::circle(haylen::math::Ray::between({0.0F, 0.0F}, {30.0F, 0.0F}), {{60.0F, 0.0F}, 10.0F}).has_value());
}

TEST(RaycastTest, TreatsClosedShapesAsSolid) {
    const haylen::math::Ray ray = haylen::math::Ray::between({0.0F, 0.0F}, {100.0F, 0.0F});
    const std::optional<haylen::math::RayHit> insideCircle = haylen::math::Raycast::circle(ray, {{0.0F, 0.0F}, 5.0F});
    ASSERT_TRUE(insideCircle.has_value());
    EXPECT_EQ(insideCircle->distance, 0.0F);
    EXPECT_TRUE(insideCircle->normal.isZero());

    const std::optional<haylen::math::RayHit> insideRect = haylen::math::Raycast::rect(ray, {-5.0F, -5.0F, 10.0F, 10.0F});
    ASSERT_TRUE(insideRect.has_value());
    EXPECT_TRUE(insideRect->normal.isZero());

    const std::vector<haylen::math::Vec2> diamond{{50.0F, -10.0F}, {60.0F, 0.0F}, {50.0F, 10.0F}, {40.0F, 0.0F}};
    const std::optional<haylen::math::RayHit> tip = haylen::math::Raycast::polygon(ray, diamond);
    ASSERT_TRUE(tip.has_value());
    EXPECT_FLOAT_EQ(tip->distance, 40.0F);
    const std::optional<haylen::math::RayHit> inside = haylen::math::Raycast::polygon(haylen::math::Ray::between({50.0F, 0.0F}, {100.0F, 0.0F}), diamond);
    ASSERT_TRUE(inside.has_value());
    EXPECT_TRUE(inside->normal.isZero());
    EXPECT_FALSE(haylen::math::Raycast::polygon(ray, std::vector<haylen::math::Vec2>{{0.0F, 1.0F}, {5.0F, 1.0F}}).has_value());
}

TEST(RaycastTest, CastsAgainstChainsAndSegmentLists) {
    const haylen::math::Ray ray = haylen::math::Ray::between({0.0F, 5.0F}, {100.0F, 5.0F});
    const std::vector<haylen::math::Vec2> stairs{{20.0F, 0.0F}, {20.0F, 10.0F}, {40.0F, 10.0F}, {40.0F, 0.0F}};
    const std::optional<haylen::math::RayHit> first = haylen::math::Raycast::chain(ray, stairs, false);
    const std::optional<haylen::math::RayHit> fromInside = haylen::math::Raycast::chain(haylen::math::Ray::between({30.0F, 5.0F}, {100.0F, 5.0F}), stairs, false);
    const std::optional<haylen::math::RayHit> closing = haylen::math::Raycast::chain(haylen::math::Ray::between({30.0F, 5.0F}, {30.0F, -20.0F}), stairs, true);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(fromInside.has_value());
    ASSERT_TRUE(closing.has_value());
    EXPECT_EQ(first->index, 0U);
    EXPECT_FLOAT_EQ(fromInside->distance, 10.0F);
    EXPECT_EQ(closing->index, 3U);
    EXPECT_FALSE(haylen::math::Raycast::chain(haylen::math::Ray::between({30.0F, 5.0F}, {30.0F, -20.0F}), stairs, false).has_value());
    EXPECT_FALSE(haylen::math::Raycast::chain(ray, std::vector<haylen::math::Vec2>{{30.0F, 5.0F}}, true).has_value()) << "A single point has no edge to hit.";

    const std::vector<haylen::math::Segment> fences{{{60.0F, 0.0F}, {60.0F, 10.0F}}, {{30.0F, 0.0F}, {30.0F, 10.0F}}, {{90.0F, 0.0F}, {90.0F, 10.0F}}};
    const std::optional<haylen::math::RayHit> closest = haylen::math::Raycast::segments(ray, fences);
    ASSERT_TRUE(closest.has_value());
    EXPECT_EQ(closest->index, 1U);

    std::vector<haylen::math::RayHit> hits;
    haylen::math::Raycast::segmentsAll(ray, fences, 0, hits);
    ASSERT_EQ(hits.size(), 3U);
    EXPECT_EQ(hits[0].index, 1U);
    EXPECT_EQ(hits[1].index, 0U);
    EXPECT_EQ(hits[2].index, 2U);
    haylen::math::Raycast::segmentsAll(ray, fences, 2, hits);
    EXPECT_EQ(hits.size(), 2U);

    // A ray that starts at NaN hits nothing instead of reporting distances that cannot be sorted.
    haylen::math::Raycast::segmentsAll(haylen::math::Ray{{std::numeric_limits<float>::quiet_NaN(), 5.0F}, {1.0F, 0.0F}, 100.0F}, fences, 0, hits);
    EXPECT_TRUE(hits.empty());
}

TEST(RaycastTest, BouncesAndFansAnyCast) {
    // A ray between two parallel mirrors bounces back and forth until it runs out of bounces.
    const std::vector<haylen::math::Segment> mirrors{{{100.0F, -50.0F}, {100.0F, 50.0F}}, {{0.0F, -50.0F}, {0.0F, 50.0F}}};
    const auto cast = [&mirrors](const haylen::math::Ray& ray) { return haylen::math::Raycast::segments(ray, mirrors); };
    std::vector<haylen::math::RayHit> hits;
    const haylen::math::Ray last = haylen::math::Raycast::bounce(haylen::math::Ray{{50.0F, 0.0F}, {1.0F, 0.0F}, 1000.0F}, 3, cast, hits);
    ASSERT_EQ(hits.size(), 4U);
    EXPECT_EQ(hits[0].index, 0U);
    EXPECT_EQ(hits[1].index, 1U);
    EXPECT_NEAR(last.getEnd().x, 0.0F, 0.01F);

    // A path that runs out of length ends in the open, where the last leg stops.
    const haylen::math::Ray shortPath = haylen::math::Raycast::bounce(haylen::math::Ray{{50.0F, 0.0F}, {1.0F, 0.0F}, 120.0F}, 5, cast, hits);
    EXPECT_EQ(hits.size(), 1U);
    EXPECT_NEAR(shortPath.getEnd().x, 30.0F, 0.01F);

    std::vector<std::optional<haylen::math::RayHit>> fan;
    haylen::math::Raycast::fan({50.0F, 0.0F}, 0.0F, std::numbers::pi_v<float>, 3, 200.0F, cast, fan);
    ASSERT_EQ(fan.size(), 3U);
    EXPECT_FALSE(fan[0].has_value());
    ASSERT_TRUE(fan[1].has_value());
    EXPECT_EQ(fan[1]->index, 0U);
    EXPECT_FALSE(fan[2].has_value());
    haylen::math::Raycast::fan({50.0F, 0.0F}, std::numbers::pi_v<float>, 1.0F, 1, 200.0F, cast, fan);
    ASSERT_FALSE(fan.empty());
    ASSERT_TRUE(fan.front().has_value());
    EXPECT_EQ(fan.front()->index, 1U);
}

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
