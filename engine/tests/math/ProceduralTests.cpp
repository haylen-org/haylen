#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/math/Easing.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Noise2D.hpp"
#include "haylen/math/PoissonDisk.hpp"
#include "haylen/math/Random.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::math {

TEST(EasingTest, EveryCurveStartsAtZeroAndEndsAtOne) {
    for (int index = 0; index <= static_cast<int>(Easing::Type::BounceInOut); ++index) {
        const auto curve = static_cast<Easing::Type>(index);
        EXPECT_NEAR(Easing::apply(curve, 0.0F), 0.0F, 1e-4F) << index;
        EXPECT_NEAR(Easing::apply(curve, 1.0F), 1.0F, 1e-4F) << index;
        EXPECT_NEAR(Easing::apply(curve, -1.0F), 0.0F, 1e-4F) << index;
        EXPECT_NEAR(Easing::apply(curve, 2.0F), 1.0F, 1e-4F) << index;
    }
}

TEST(EasingTest, CurvesHaveTheirCharacteristicShape) {
    EXPECT_FLOAT_EQ(Easing::apply(Easing::Type::Linear, 0.3F), 0.3F);
    EXPECT_LT(Easing::apply(Easing::Type::QuadIn, 0.5F), 0.5F);
    EXPECT_GT(Easing::apply(Easing::Type::QuadOut, 0.5F), 0.5F);
    EXPECT_NEAR(Easing::apply(Easing::Type::CubicInOut, 0.5F), 0.5F, 1e-5F);
    EXPECT_LT(Easing::apply(Easing::Type::BackIn, 0.2F), 0.0F);
    EXPECT_GT(Easing::apply(Easing::Type::BackOut, 0.8F), 1.0F);
    EXPECT_GT(Easing::apply(Easing::Type::ElasticOut, 0.1F), 1.0F);

    for (const float t : {0.1F, 0.3F, 0.5F, 0.7F, 0.95F}) {
        EXPECT_GE(Easing::apply(Easing::Type::BounceOut, t), 0.0F);
        EXPECT_LE(Easing::apply(Easing::Type::BounceOut, t), 1.0F);
    }
}

TEST(EasingTest, ResolvesNames) {
    EXPECT_EQ(Easing::parse("linear"), Easing::Type::Linear);
    EXPECT_EQ(Easing::parse("elastic_in_out"), Easing::Type::ElasticInOut);
    EXPECT_FALSE(Easing::parse("unknown").has_value());
}

TEST(RandomTest, IsDeterministicPerSeed) {
    Random first(42);
    Random second(42);
    for (int index = 0; index < 100; ++index) {
        EXPECT_EQ(first.nextU64(), second.nextU64());
    }

    second.reseed(7);
    Random third(7);
    EXPECT_EQ(second.nextU32(), third.nextU32());
}

TEST(RandomTest, RangesStayInBounds) {
    Random random(1);
    for (int index = 0; index < 1000; ++index) {
        const float value = random.nextFloat();
        EXPECT_GE(value, 0.0F);
        EXPECT_LT(value, 1.0F);

        const int integer = random.range(-3, 3);
        EXPECT_GE(integer, -3);
        EXPECT_LE(integer, 3);

        const float real = random.range(5.0F, 6.0F);
        EXPECT_GE(real, 5.0F);
        EXPECT_LE(real, 6.0F);
    }
    EXPECT_EQ(random.range(5, 5), 5);
    EXPECT_EQ(random.range(9, 2), 9);
    EXPECT_FALSE(random.chance(0.0F));
    EXPECT_TRUE(random.chance(1.0F));
}

TEST(RandomTest, WeightedIndexFollowsWeights) {
    Random random(3);
    const std::array<float, 3> weights{0.0F, 1.0F, 0.0F};
    for (int index = 0; index < 50; ++index) {
        EXPECT_EQ(random.weightedIndex(weights), 1U);
    }
    EXPECT_THROW((void)random.weightedIndex(std::span<const float>{}), std::invalid_argument);
    EXPECT_THROW((void)random.weightedIndex(std::array<float, 2>{0.0F, 0.0F}), std::invalid_argument);
    EXPECT_THROW((void)random.weightedIndex(std::array<float, 2>{2.0F, -1.0F}), std::invalid_argument);
    EXPECT_THROW((void)random.weightedIndex(std::array<float, 2>{1.0F, std::numeric_limits<float>::quiet_NaN()}), std::invalid_argument);
}

TEST(RandomTest, ShufflePreservesElements) {
    Random random(9);
    std::array<int, 8> values{1, 2, 3, 4, 5, 6, 7, 8};
    random.shuffle(std::span<int>(values));
    std::sort(values.begin(), values.end());
    EXPECT_EQ(values, (std::array<int, 8>{1, 2, 3, 4, 5, 6, 7, 8}));
}

TEST(NoiseTest, IsDeterministicAndBounded) {
    const Noise2D noise(12);
    const Noise2D same(12);
    for (float x = -5.0F; x < 5.0F; x += 0.37F) {
        for (float y = -5.0F; y < 5.0F; y += 0.41F) {
            EXPECT_EQ(noise.simplex(x, y), same.simplex(x, y));
            EXPECT_GE(noise.perlin(x, y), -1.5F);
            EXPECT_LE(noise.perlin(x, y), 1.5F);
            EXPECT_GE(noise.fractal(x, y), -1.0F);
            EXPECT_LE(noise.fractal(x, y), 1.0F);
        }
    }
    EXPECT_EQ(noise.fractal(1.0F, 1.0F, 0), 0.0F);
    EXPECT_NE(Noise2D(1).simplex(0.3F, 0.7F), Noise2D(2).simplex(0.3F, 0.7F));
}

TEST(GeometryTest, CirclesAndSegments) {
    const Circle circle{Vec2{}, 5.0F};
    EXPECT_TRUE(circle.contains(Vec2(3.0F, 4.0F)));
    EXPECT_FALSE(circle.contains(Vec2(4.0F, 4.0F)));
    EXPECT_EQ(circle.getBounds(), (Rect{-5.0F, -5.0F, 10.0F, 10.0F}));
    EXPECT_TRUE(Geometry::intersects(circle, Circle{Vec2(9.0F, 0.0F), 4.0F}));
    EXPECT_FALSE(Geometry::intersects(circle, Circle{Vec2(10.0F, 0.0F), 4.0F}));
    EXPECT_TRUE(Geometry::intersects(circle, Rect{4.0F, -1.0F, 2.0F, 2.0F}));
    EXPECT_FALSE(Geometry::intersects(circle, Rect{6.0F, 6.0F, 2.0F, 2.0F}));

    const Segment horizontal{Vec2(-1.0F, 0.0F), Vec2(1.0F, 0.0F)};
    const Segment vertical{Vec2(0.0F, -1.0F), Vec2(0.0F, 1.0F)};
    EXPECT_EQ(Geometry::intersection(horizontal, vertical), Vec2{});
    EXPECT_FALSE(Geometry::intersection(horizontal, Segment{Vec2(-1.0F, 1.0F), Vec2(1.0F, 1.0F)}).has_value());
    EXPECT_FALSE(Geometry::intersection(horizontal, Segment{Vec2(2.0F, -1.0F), Vec2(2.0F, 1.0F)}).has_value());
    EXPECT_EQ(Geometry::closestPoint(horizontal, Vec2(5.0F, 3.0F)), Vec2(1.0F, 0.0F));
    EXPECT_EQ(Geometry::closestPoint(Segment{Vec2(1.0F, 1.0F), Vec2(1.0F, 1.0F)}, Vec2{}), Vec2(1.0F, 1.0F));
    EXPECT_FLOAT_EQ(Geometry::distanceToSegment(horizontal, Vec2(0.0F, 2.0F)), 2.0F);
}

TEST(GeometryTest, Polygons) {
    const std::array<Vec2, 4> square{Vec2(0.0F, 0.0F), Vec2(10.0F, 0.0F), Vec2(10.0F, 10.0F), Vec2(0.0F, 10.0F)};
    EXPECT_TRUE(Geometry::contains(square, Vec2(5.0F, 5.0F)));
    EXPECT_FALSE(Geometry::contains(square, Vec2(15.0F, 5.0F)));
    EXPECT_FLOAT_EQ(Geometry::signedArea(square), 100.0F);
    EXPECT_EQ(Geometry::centroid(square), Vec2(5.0F, 5.0F));
    EXPECT_TRUE(Geometry::isConvex(square));
    EXPECT_EQ(Geometry::bounds(square), (Rect{0.0F, 0.0F, 10.0F, 10.0F}));
    EXPECT_EQ(Geometry::bounds(std::span<const Vec2>{}), Rect{});

    const std::array<Vec2, 5> arrow{Vec2(0.0F, 0.0F), Vec2(10.0F, 0.0F), Vec2(5.0F, 3.0F), Vec2(10.0F, 10.0F), Vec2(0.0F, 10.0F)};
    EXPECT_FALSE(Geometry::isConvex(arrow));
    EXPECT_FALSE(Geometry::isConvex(std::span<const Vec2>(square.data(), 2)));

    const std::array<Vec2, 3> line{Vec2(0.0F, 0.0F), Vec2(1.0F, 1.0F), Vec2(2.0F, 2.0F)};
    EXPECT_EQ(Geometry::centroid(line), Vec2(1.0F, 1.0F));
    EXPECT_EQ(Geometry::centroid(std::span<const Vec2>{}), Vec2{});
}

TEST(GeometryTest, StarsThatTurnOneWayAreNotConvex) {
    std::vector<Vec2> hexagon;
    std::vector<Vec2> pentagram;
    for (int index = 0; index < 6; ++index) {
        hexagon.push_back(Vec2::fromAngle(Math::kTau * static_cast<float>(index) / 6.0F, 10.0F));
    }
    for (int index = 0; index < 5; ++index) {
        pentagram.push_back(Vec2::fromAngle(Math::kTau * static_cast<float>(index * 2) / 5.0F, 10.0F));
    }

    EXPECT_TRUE(Geometry::isConvex(hexagon));
    EXPECT_FALSE(Geometry::isConvex(pentagram));
    std::reverse(hexagon.begin(), hexagon.end());
    std::reverse(pentagram.begin(), pentagram.end());
    EXPECT_TRUE(Geometry::isConvex(hexagon));
    EXPECT_FALSE(Geometry::isConvex(pentagram));
}

TEST(GeometryTest, ConvexHullDropsInteriorPoints) {
    const std::array<Vec2, 6> points{Vec2(0.0F, 0.0F), Vec2(4.0F, 0.0F), Vec2(4.0F, 4.0F), Vec2(0.0F, 4.0F), Vec2(2.0F, 2.0F), Vec2(4.0F, 0.0F)};
    const std::vector<Vec2> hull = Geometry::convexHull(points);
    EXPECT_EQ(hull.size(), 4U);
    EXPECT_GT(Geometry::signedArea(hull), 0.0F);
    EXPECT_EQ(Geometry::convexHull(std::span<const Vec2>(points.data(), 2)).size(), 2U);

    // A point with a NaN coordinate is left out instead of breaking the sort.
    const std::array<Vec2, 5> withNaN{Vec2(0.0F, 0.0F), Vec2(4.0F, 0.0F), Vec2(std::numeric_limits<float>::quiet_NaN(), 2.0F), Vec2(4.0F, 4.0F), Vec2(0.0F, 4.0F)};
    EXPECT_EQ(Geometry::convexHull(withNaN).size(), 4U);
}

TEST(GeometryTest, TriangulatesConcavePolygonsInEitherWinding) {
    std::vector<Vec2> arrow{Vec2(0.0F, 0.0F), Vec2(10.0F, 0.0F), Vec2(5.0F, 3.0F), Vec2(10.0F, 10.0F), Vec2(0.0F, 10.0F)};
    const auto triangles = Geometry::triangulate(arrow);
    ASSERT_EQ(triangles.size(), 9U);

    float area = 0.0F;
    for (std::size_t index = 0; index < triangles.size(); index += 3) {
        const std::array<Vec2, 3> triangle{arrow[triangles[index]], arrow[triangles[index + 1]], arrow[triangles[index + 2]]};
        EXPECT_GT(Geometry::signedArea(triangle), 0.0F);
        area += Geometry::signedArea(triangle);
    }
    EXPECT_NEAR(area, Geometry::signedArea(arrow), 1e-3F);

    std::reverse(arrow.begin(), arrow.end());
    EXPECT_EQ(Geometry::triangulate(arrow).size(), 9U);
    EXPECT_TRUE(Geometry::triangulate(std::span<const Vec2>(arrow.data(), 2)).empty());
}

TEST(PoissonDiskTest, KeepsMinimumDistanceAndPredicate) {
    Random random(5);
    const PoissonDisk::Options options{
        .area = Rect{0.0F, 0.0F, 500.0F, 300.0F},
        .minimumDistance = 40.0F,
        .accept = [](Vec2 point) { return point.x < 400.0F; },
    };
    const std::vector<Vec2> points = PoissonDisk::sample(options, random);

    ASSERT_GT(points.size(), 20U);
    for (std::size_t first = 0; first < points.size(); ++first) {
        EXPECT_LT(points[first].x, 400.0F);
        EXPECT_TRUE(options.area.contains(points[first]));
        for (std::size_t second = first + 1; second < points.size(); ++second) {
            EXPECT_GE(Vec2::distance(points[first], points[second]), 40.0F - 1e-3F);
        }
    }
}

TEST(PoissonDiskTest, RejectsDegenerateInput) {
    Random random(5);
    EXPECT_TRUE(PoissonDisk::sample(PoissonDisk::Options{.area = Rect{}}, random).empty());
    EXPECT_TRUE(PoissonDisk::sample(PoissonDisk::Options{.area = Rect{0.0F, 0.0F, 10.0F, 10.0F}, .minimumDistance = 0.0F}, random).empty());

    const PoissonDisk::Options nothingAccepted{.area = Rect{0.0F, 0.0F, 10.0F, 10.0F}, .accept = [](Vec2) { return false; }};
    EXPECT_TRUE(PoissonDisk::sample(nothingAccepted, random).empty());
    EXPECT_THROW((void)PoissonDisk::sample(PoissonDisk::Options{.area = Rect{0.0F, 0.0F, 2000.0F, 2000.0F}, .minimumDistance = 0.01F}, random), std::invalid_argument);
}

TEST(PoissonDiskTest, BoundsHugeDistancesAndRejectsOnesThatAreNotFinite) {
    Random random(5);
    const Rect area{0.0F, 0.0F, 100.0F, 100.0F};

    // The largest distances reach far beyond any int number of cells, and the search stays within the grid.
    for (const float maximum : {1e30F, std::numeric_limits<float>::max()}) {
        const std::vector<Vec2> points = PoissonDisk::sample(PoissonDisk::Options{.area = area, .minimumDistance = 10.0F, .maximumDistance = maximum, .distance = [](Vec2) { return 10.0F; }}, random);
        ASSERT_GT(points.size(), 10U);
        for (std::size_t first = 0; first < points.size(); ++first) {
            for (std::size_t second = first + 1; second < points.size(); ++second) {
                EXPECT_GE(Vec2::distance(points[first], points[second]), 10.0F - 1e-3F);
            }
        }
    }

    const float infinity = std::numeric_limits<float>::infinity();
    EXPECT_THROW((void)PoissonDisk::sample(PoissonDisk::Options{.area = area, .minimumDistance = 10.0F, .maximumDistance = infinity, .distance = [](Vec2) { return 10.0F; }}, random), std::invalid_argument);
    EXPECT_THROW((void)PoissonDisk::sample(PoissonDisk::Options{.area = area, .minimumDistance = 10.0F, .maximumDistance = 20.0F, .distance = [](Vec2) { return std::numeric_limits<float>::quiet_NaN(); }}, random), std::invalid_argument);
}

TEST(MathArgumentsLuaTest, HandleNaNAndRejectInvalidRangesAndWeights) {
    test::EngineFixture fixture;
    fixture.runLua("m = require('haylen.math')");

    EXPECT_EQ(fixture.lua("return m.ease({points = {0, 1}}, 0 / 0) .. ' ' .. m.ease({points = {0, 1}}, 0.25)"), "0.0 0.25");
    EXPECT_NE(fixture.lua("return m.clamp(5, 1, 0)").find("bad argument #3 to 'clamp' (expected a maximum of at least the minimum)"), std::string::npos);
    EXPECT_NE(fixture.lua("return m.random(1):pick({1, -1})").find("Weights must be finite and not negative."), std::string::npos);
    EXPECT_NE(fixture.lua("return m.random(1):pick({1, 0 / 0})").find("Weights must be finite and not negative."), std::string::npos);
    EXPECT_NE(fixture.lua("return m.random(1):pick({0, 0})").find("A weighted pick needs at least one positive weight."), std::string::npos);
    EXPECT_NE(fixture.lua("return m.polygon.decompose({{0, 0}, {4, 0}, {4, 4}}, -1)").find("bad argument #2 to 'decompose' (expected a non-negative integer)"), std::string::npos);
}

} // namespace haylen::math
