#include <gtest/gtest.h>

#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "haylen/math/Ray.hpp"
#include "haylen/math/Raycast.hpp"
#include "support/EngineFixture.hpp"

namespace haylen {

TEST(RaycastTest, HitsSegmentsRectsAndCirclesWithFacingNormals) {
    const math::Ray ray = math::Ray::between({0.0F, 0.0F}, {100.0F, 0.0F});

    const std::optional<math::RayHit> wall = math::Raycast::segment(ray, {{50.0F, -10.0F}, {50.0F, 10.0F}});
    ASSERT_TRUE(wall.has_value());
    EXPECT_FLOAT_EQ(wall->distance, 50.0F);
    EXPECT_EQ(wall->normal, (math::Vec2{-1.0F, 0.0F}));
    EXPECT_FALSE(math::Raycast::segment(ray, {{150.0F, -10.0F}, {150.0F, 10.0F}}).has_value());
    EXPECT_FALSE(math::Raycast::segment(ray, {{10.0F, 0.0F}, {20.0F, 0.0F}}).has_value());

    const std::optional<math::RayHit> box = math::Raycast::rect(ray, {40.0F, -5.0F, 10.0F, 10.0F});
    ASSERT_TRUE(box.has_value());
    EXPECT_EQ(box->point, (math::Vec2{40.0F, 0.0F}));
    EXPECT_EQ(box->normal, (math::Vec2{-1.0F, 0.0F}));
    const std::optional<math::RayHit> fromBelow = math::Raycast::rect(math::Ray::between({45.0F, 50.0F}, {45.0F, 0.0F}), {40.0F, -5.0F, 10.0F, 10.0F});
    ASSERT_TRUE(fromBelow.has_value());
    EXPECT_EQ(fromBelow->normal, (math::Vec2{0.0F, 1.0F}));
    EXPECT_FLOAT_EQ(fromBelow->distance, 45.0F);
    EXPECT_FALSE(math::Raycast::rect(ray, {40.0F, 5.0F, 10.0F, 10.0F}).has_value());

    const std::optional<math::RayHit> ball = math::Raycast::circle(ray, {{60.0F, 0.0F}, 10.0F});
    ASSERT_TRUE(ball.has_value());
    EXPECT_FLOAT_EQ(ball->distance, 50.0F);
    EXPECT_EQ(ball->normal, (math::Vec2{-1.0F, 0.0F}));
    EXPECT_FALSE(math::Raycast::circle(ray, {{-60.0F, 0.0F}, 10.0F}).has_value());
    EXPECT_FALSE(math::Raycast::circle(ray, {{60.0F, 20.0F}, 10.0F}).has_value());
    EXPECT_FALSE(math::Raycast::circle(math::Ray::between({0.0F, 0.0F}, {30.0F, 0.0F}), {{60.0F, 0.0F}, 10.0F}).has_value());
}

TEST(RaycastTest, TreatsClosedShapesAsSolid) {
    const math::Ray ray = math::Ray::between({0.0F, 0.0F}, {100.0F, 0.0F});
    const std::optional<math::RayHit> insideCircle = math::Raycast::circle(ray, {{0.0F, 0.0F}, 5.0F});
    ASSERT_TRUE(insideCircle.has_value());
    EXPECT_EQ(insideCircle->distance, 0.0F);
    EXPECT_TRUE(insideCircle->normal.isZero());

    const std::optional<math::RayHit> insideRect = math::Raycast::rect(ray, {-5.0F, -5.0F, 10.0F, 10.0F});
    ASSERT_TRUE(insideRect.has_value());
    EXPECT_TRUE(insideRect->normal.isZero());

    const std::vector<math::Vec2> diamond{{50.0F, -10.0F}, {60.0F, 0.0F}, {50.0F, 10.0F}, {40.0F, 0.0F}};
    const std::optional<math::RayHit> tip = math::Raycast::polygon(ray, diamond);
    ASSERT_TRUE(tip.has_value());
    EXPECT_FLOAT_EQ(tip->distance, 40.0F);
    EXPECT_TRUE(math::Raycast::polygon(math::Ray::between({50.0F, 0.0F}, {100.0F, 0.0F}), diamond)->normal.isZero());
    EXPECT_FALSE(math::Raycast::polygon(ray, std::vector<math::Vec2>{{0.0F, 1.0F}, {5.0F, 1.0F}}).has_value());
}

TEST(RaycastTest, CastsAgainstChainsAndSegmentLists) {
    const math::Ray ray = math::Ray::between({0.0F, 5.0F}, {100.0F, 5.0F});
    const std::vector<math::Vec2> stairs{{20.0F, 0.0F}, {20.0F, 10.0F}, {40.0F, 10.0F}, {40.0F, 0.0F}};
    EXPECT_EQ(math::Raycast::chain(ray, stairs, false)->index, 0U);
    EXPECT_FLOAT_EQ(math::Raycast::chain(math::Ray::between({30.0F, 5.0F}, {100.0F, 5.0F}), stairs, false)->distance, 10.0F);
    EXPECT_EQ(math::Raycast::chain(math::Ray::between({30.0F, 5.0F}, {30.0F, -20.0F}), stairs, true)->index, 3U);
    EXPECT_FALSE(math::Raycast::chain(math::Ray::between({30.0F, 5.0F}, {30.0F, -20.0F}), stairs, false).has_value());

    const std::vector<math::Segment> fences{{{60.0F, 0.0F}, {60.0F, 10.0F}}, {{30.0F, 0.0F}, {30.0F, 10.0F}}, {{90.0F, 0.0F}, {90.0F, 10.0F}}};
    const std::optional<math::RayHit> closest = math::Raycast::segments(ray, fences);
    ASSERT_TRUE(closest.has_value());
    EXPECT_EQ(closest->index, 1U);

    std::vector<math::RayHit> hits;
    math::Raycast::segmentsAll(ray, fences, 0, hits);
    ASSERT_EQ(hits.size(), 3U);
    EXPECT_EQ(hits[0].index, 1U);
    EXPECT_EQ(hits[1].index, 0U);
    EXPECT_EQ(hits[2].index, 2U);
    math::Raycast::segmentsAll(ray, fences, 2, hits);
    EXPECT_EQ(hits.size(), 2U);

    // A ray that starts at NaN hits nothing instead of reporting distances that cannot be sorted.
    math::Raycast::segmentsAll(math::Ray{{std::numeric_limits<float>::quiet_NaN(), 5.0F}, {1.0F, 0.0F}, 100.0F}, fences, 0, hits);
    EXPECT_TRUE(hits.empty());
}

TEST(RaycastTest, BouncesAndFansAnyCast) {
    // A ray between two parallel mirrors bounces back and forth until it runs out of bounces.
    const std::vector<math::Segment> mirrors{{{100.0F, -50.0F}, {100.0F, 50.0F}}, {{0.0F, -50.0F}, {0.0F, 50.0F}}};
    const auto cast = [&mirrors](const math::Ray& ray) { return math::Raycast::segments(ray, mirrors); };
    std::vector<math::RayHit> hits;
    const math::Ray last = math::Raycast::bounce(math::Ray{{50.0F, 0.0F}, {1.0F, 0.0F}, 1000.0F}, 3, cast, hits);
    ASSERT_EQ(hits.size(), 4U);
    EXPECT_EQ(hits[0].index, 0U);
    EXPECT_EQ(hits[1].index, 1U);
    EXPECT_NEAR(last.getEnd().x, 0.0F, 0.01F);

    // A path that runs out of length ends in the open, where the last leg stops.
    const math::Ray shortPath = math::Raycast::bounce(math::Ray{{50.0F, 0.0F}, {1.0F, 0.0F}, 120.0F}, 5, cast, hits);
    EXPECT_EQ(hits.size(), 1U);
    EXPECT_NEAR(shortPath.getEnd().x, 30.0F, 0.01F);

    std::vector<std::optional<math::RayHit>> fan;
    math::Raycast::fan({50.0F, 0.0F}, 0.0F, std::numbers::pi_v<float>, 3, 200.0F, cast, fan);
    ASSERT_EQ(fan.size(), 3U);
    EXPECT_FALSE(fan[0].has_value());
    EXPECT_EQ(fan[1]->index, 0U);
    EXPECT_FALSE(fan[2].has_value());
    math::Raycast::fan({50.0F, 0.0F}, std::numbers::pi_v<float>, 1.0F, 1, 200.0F, cast, fan);
    EXPECT_EQ(fan.front()->index, 1U);
}

TEST(RaycastLuaTest, CastsRaysFromLua) {
    test::EngineFixture fixture;
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

} // namespace haylen
