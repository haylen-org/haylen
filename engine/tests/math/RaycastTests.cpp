#include <gtest/gtest.h>

#include <limits>
#include <numbers>
#include <optional>
#include <vector>

#include "haylen/math/Ray2D.hpp"
#include "haylen/math/Raycast.hpp"

namespace haylen::math {

TEST(RaycastTest, HitsSegmentsRectsAndCirclesWithFacingNormals) {
    const Ray2D ray = Ray2D::between({0.0F, 0.0F}, {100.0F, 0.0F});

    const std::optional<RayHit2D> wall = Raycast::segment(ray, {{50.0F, -10.0F}, {50.0F, 10.0F}});
    ASSERT_TRUE(wall.has_value());
    EXPECT_FLOAT_EQ(wall->distance, 50.0F);
    EXPECT_EQ(wall->normal, (Vec2{-1.0F, 0.0F}));
    EXPECT_FALSE(Raycast::segment(ray, {{150.0F, -10.0F}, {150.0F, 10.0F}}).has_value());
    EXPECT_FALSE(Raycast::segment(ray, {{10.0F, 0.0F}, {20.0F, 0.0F}}).has_value());

    const std::optional<RayHit2D> box = Raycast::rect(ray, {40.0F, -5.0F, 10.0F, 10.0F});
    ASSERT_TRUE(box.has_value());
    EXPECT_EQ(box->point, (Vec2{40.0F, 0.0F}));
    EXPECT_EQ(box->normal, (Vec2{-1.0F, 0.0F}));
    const std::optional<RayHit2D> fromBelow = Raycast::rect(Ray2D::between({45.0F, 50.0F}, {45.0F, 0.0F}), {40.0F, -5.0F, 10.0F, 10.0F});
    ASSERT_TRUE(fromBelow.has_value());
    EXPECT_EQ(fromBelow->normal, (Vec2{0.0F, 1.0F}));
    EXPECT_FLOAT_EQ(fromBelow->distance, 45.0F);
    EXPECT_FALSE(Raycast::rect(ray, {40.0F, 5.0F, 10.0F, 10.0F}).has_value());

    const std::optional<RayHit2D> ball = Raycast::circle(ray, {{60.0F, 0.0F}, 10.0F});
    ASSERT_TRUE(ball.has_value());
    EXPECT_FLOAT_EQ(ball->distance, 50.0F);
    EXPECT_EQ(ball->normal, (Vec2{-1.0F, 0.0F}));
    EXPECT_FALSE(Raycast::circle(ray, {{-60.0F, 0.0F}, 10.0F}).has_value());
    EXPECT_FALSE(Raycast::circle(ray, {{60.0F, 20.0F}, 10.0F}).has_value());
    EXPECT_FALSE(Raycast::circle(Ray2D::between({0.0F, 0.0F}, {30.0F, 0.0F}), {{60.0F, 0.0F}, 10.0F}).has_value());

    // A circle without a radius is a point, which faces the ray that strikes it.
    const std::optional<RayHit2D> dot = Raycast::circle(ray, {{60.0F, 0.0F}, 0.0F});
    ASSERT_TRUE(dot.has_value());
    EXPECT_FLOAT_EQ(dot->distance, 60.0F);
    EXPECT_EQ(dot->normal, (Vec2{-1.0F, 0.0F}));
}

TEST(RaycastTest, TreatsClosedShapesAsSolid) {
    const Ray2D ray = Ray2D::between({0.0F, 0.0F}, {100.0F, 0.0F});
    const std::optional<RayHit2D> insideCircle = Raycast::circle(ray, {{0.0F, 0.0F}, 5.0F});
    ASSERT_TRUE(insideCircle.has_value());
    EXPECT_EQ(insideCircle->distance, 0.0F);
    EXPECT_TRUE(insideCircle->normal.isZero());

    const std::optional<RayHit2D> insideRect = Raycast::rect(ray, {-5.0F, -5.0F, 10.0F, 10.0F});
    ASSERT_TRUE(insideRect.has_value());
    EXPECT_TRUE(insideRect->normal.isZero());

    const std::vector<Vec2> diamond{{50.0F, -10.0F}, {60.0F, 0.0F}, {50.0F, 10.0F}, {40.0F, 0.0F}};
    const std::optional<RayHit2D> tip = Raycast::polygon(ray, diamond);
    ASSERT_TRUE(tip.has_value());
    EXPECT_FLOAT_EQ(tip->distance, 40.0F);
    const std::optional<RayHit2D> inside = Raycast::polygon(Ray2D::between({50.0F, 0.0F}, {100.0F, 0.0F}), diamond);
    ASSERT_TRUE(inside.has_value());
    EXPECT_TRUE(inside->normal.isZero());
    EXPECT_FALSE(Raycast::polygon(ray, std::vector<Vec2>{{0.0F, 1.0F}, {5.0F, 1.0F}}).has_value());
}

TEST(RaycastTest, CastsAgainstChainsAndSegmentLists) {
    const Ray2D ray = Ray2D::between({0.0F, 5.0F}, {100.0F, 5.0F});
    const std::vector<Vec2> stairs{{20.0F, 0.0F}, {20.0F, 10.0F}, {40.0F, 10.0F}, {40.0F, 0.0F}};
    const std::optional<RayHit2D> first = Raycast::chain(ray, stairs, false);
    const std::optional<RayHit2D> fromInside = Raycast::chain(Ray2D::between({30.0F, 5.0F}, {100.0F, 5.0F}), stairs, false);
    const std::optional<RayHit2D> closing = Raycast::chain(Ray2D::between({30.0F, 5.0F}, {30.0F, -20.0F}), stairs, true);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(fromInside.has_value());
    ASSERT_TRUE(closing.has_value());
    EXPECT_EQ(first->index, 0U);
    EXPECT_FLOAT_EQ(fromInside->distance, 10.0F);
    EXPECT_EQ(closing->index, 3U);
    EXPECT_FALSE(Raycast::chain(Ray2D::between({30.0F, 5.0F}, {30.0F, -20.0F}), stairs, false).has_value());
    EXPECT_FALSE(Raycast::chain(ray, std::vector<Vec2>{{30.0F, 5.0F}}, true).has_value()) << "A single point has no edge to hit.";

    const std::vector<Segment2D> fences{{{60.0F, 0.0F}, {60.0F, 10.0F}}, {{30.0F, 0.0F}, {30.0F, 10.0F}}, {{90.0F, 0.0F}, {90.0F, 10.0F}}};
    const std::optional<RayHit2D> closest = Raycast::segments(ray, fences);
    ASSERT_TRUE(closest.has_value());
    EXPECT_EQ(closest->index, 1U);

    std::vector<RayHit2D> hits;
    Raycast::segmentsAll(ray, fences, 0, hits);
    ASSERT_EQ(hits.size(), 3U);
    EXPECT_EQ(hits[0].index, 1U);
    EXPECT_EQ(hits[1].index, 0U);
    EXPECT_EQ(hits[2].index, 2U);
    Raycast::segmentsAll(ray, fences, 2, hits);
    EXPECT_EQ(hits.size(), 2U);

    // A ray that starts at NaN hits nothing instead of reporting distances that cannot be sorted.
    Raycast::segmentsAll(Ray2D{{std::numeric_limits<float>::quiet_NaN(), 5.0F}, {1.0F, 0.0F}, 100.0F}, fences, 0, hits);
    EXPECT_TRUE(hits.empty());
}

TEST(RaycastTest, BouncesAndFansAnyCast) {
    // A ray between two parallel mirrors bounces back and forth until it runs out of bounces.
    const std::vector<Segment2D> mirrors{{{100.0F, -50.0F}, {100.0F, 50.0F}}, {{0.0F, -50.0F}, {0.0F, 50.0F}}};
    const auto cast = [&mirrors](const Ray2D& ray) { return Raycast::segments(ray, mirrors); };
    std::vector<RayHit2D> hits;
    const Ray2D last = Raycast::bounce(Ray2D{{50.0F, 0.0F}, {1.0F, 0.0F}, 1000.0F}, 3, cast, hits);
    ASSERT_EQ(hits.size(), 4U);
    EXPECT_EQ(hits[0].index, 0U);
    EXPECT_EQ(hits[1].index, 1U);
    EXPECT_NEAR(last.getEnd().x, 0.0F, 0.01F);

    // A path that runs out of length ends in the open, where the last leg stops.
    const Ray2D shortPath = Raycast::bounce(Ray2D{{50.0F, 0.0F}, {1.0F, 0.0F}, 120.0F}, 5, cast, hits);
    EXPECT_EQ(hits.size(), 1U);
    EXPECT_NEAR(shortPath.getEnd().x, 30.0F, 0.01F);

    std::vector<std::optional<RayHit2D>> fan;
    Raycast::fan({50.0F, 0.0F}, 0.0F, std::numbers::pi_v<float>, 3, 200.0F, cast, fan);
    ASSERT_EQ(fan.size(), 3U);
    EXPECT_FALSE(fan[0].has_value());
    ASSERT_TRUE(fan[1].has_value());
    EXPECT_EQ(fan[1]->index, 0U);
    EXPECT_FALSE(fan[2].has_value());
    Raycast::fan({50.0F, 0.0F}, std::numbers::pi_v<float>, 1.0F, 1, 200.0F, cast, fan);
    ASSERT_FALSE(fan.empty());
    ASSERT_TRUE(fan.front().has_value());
    EXPECT_EQ(fan.front()->index, 1U);
}

} // namespace haylen::math
