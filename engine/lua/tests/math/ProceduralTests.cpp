#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
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
#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

TEST(EasingTest, EveryCurveStartsAtZeroAndEndsAtOne) {
    for (int index = 0; index <= static_cast<int>(haylen::math::Easing::Type::BounceInOut); ++index) {
        const auto curve = static_cast<haylen::math::Easing::Type>(index);
        EXPECT_NEAR(haylen::math::Easing::apply(curve, 0.0F), 0.0F, 1e-4F) << index;
        EXPECT_NEAR(haylen::math::Easing::apply(curve, 1.0F), 1.0F, 1e-4F) << index;
        EXPECT_NEAR(haylen::math::Easing::apply(curve, -1.0F), 0.0F, 1e-4F) << index;
        EXPECT_NEAR(haylen::math::Easing::apply(curve, 2.0F), 1.0F, 1e-4F) << index;
    }
}

TEST(EasingTest, CurvesHaveTheirCharacteristicShape) {
    EXPECT_FLOAT_EQ(haylen::math::Easing::apply(haylen::math::Easing::Type::Linear, 0.3F), 0.3F);
    EXPECT_LT(haylen::math::Easing::apply(haylen::math::Easing::Type::QuadIn, 0.5F), 0.5F);
    EXPECT_GT(haylen::math::Easing::apply(haylen::math::Easing::Type::QuadOut, 0.5F), 0.5F);
    EXPECT_NEAR(haylen::math::Easing::apply(haylen::math::Easing::Type::CubicInOut, 0.5F), 0.5F, 1e-5F);
    EXPECT_LT(haylen::math::Easing::apply(haylen::math::Easing::Type::BackIn, 0.2F), 0.0F);
    EXPECT_GT(haylen::math::Easing::apply(haylen::math::Easing::Type::BackOut, 0.8F), 1.0F);
    EXPECT_GT(haylen::math::Easing::apply(haylen::math::Easing::Type::ElasticOut, 0.1F), 1.0F);

    for (const float t : {0.1F, 0.3F, 0.5F, 0.7F, 0.95F}) {
        EXPECT_GE(haylen::math::Easing::apply(haylen::math::Easing::Type::BounceOut, t), 0.0F);
        EXPECT_LE(haylen::math::Easing::apply(haylen::math::Easing::Type::BounceOut, t), 1.0F);
    }
}

TEST(EasingTest, BouncesLandOnTheEndValue) {
    for (const float landing : {1.0F, 2.0F, 2.5F}) {
        EXPECT_NEAR(haylen::math::Easing::apply(haylen::math::Easing::Type::BounceOut, landing / 2.75F), 1.0F, 1e-5F) << landing;
    }

    EXPECT_NEAR(haylen::math::Easing::apply(haylen::math::Easing::Type::BounceOut, 2.25F / 2.75F), 0.9375F, 1e-5F);
}

TEST(EasingTest, ParametersOnlyShapeTheBackAndElasticCurves) {
    EXPECT_FLOAT_EQ(haylen::math::Easing::apply(haylen::math::Easing::Type::QuadIn, 0.5F, 3.0F, 0.5F), haylen::math::Easing::apply(haylen::math::Easing::Type::QuadIn, 0.5F));
    EXPECT_LT(haylen::math::Easing::apply(haylen::math::Easing::Type::BackIn, 0.2F, 3.0F, 0.0F), haylen::math::Easing::apply(haylen::math::Easing::Type::BackIn, 0.2F));
}

TEST(EasingTest, StepsJumpWhereTheirPositionSays) {
    EXPECT_FLOAT_EQ(haylen::math::Easing::steps(0.5F, 4, haylen::math::Easing::StepPosition::End), 0.5F);
    EXPECT_FLOAT_EQ(haylen::math::Easing::steps(0.0F, 4, haylen::math::Easing::StepPosition::Start), 0.25F);
    EXPECT_FLOAT_EQ(haylen::math::Easing::steps(0.0F, 4, haylen::math::Easing::StepPosition::Both), 0.2F);
    EXPECT_FLOAT_EQ(haylen::math::Easing::steps(0.5F, 4, haylen::math::Easing::StepPosition::Both), 0.6F);
    EXPECT_FLOAT_EQ(haylen::math::Easing::steps(0.5F, 4, haylen::math::Easing::StepPosition::None), 2.0F / 3.0F);
    EXPECT_FLOAT_EQ(haylen::math::Easing::steps(1.0F, 4, haylen::math::Easing::StepPosition::None), 1.0F);
}

// The x of this curve starts flat, which stops the Newton steps at once, and bisection still finds where the curve reaches the time.
TEST(EasingTest, SolvesBezierCurvesThatStartFlat) {
    EXPECT_NEAR(haylen::math::Easing::cubicBezier(1e-4F, 0.0F, 0.0F, 0.0F, 0.0F), 1e-4F, 1e-6F);
}

TEST(EasingTest, ResolvesNames) {
    EXPECT_EQ(haylen::math::Easing::parse("linear"), haylen::math::Easing::Type::Linear);
    EXPECT_EQ(haylen::math::Easing::parse("elasticInOut"), haylen::math::Easing::Type::ElasticInOut);
    EXPECT_FALSE(haylen::math::Easing::parse("unknown").has_value());
    EXPECT_EQ(haylen::math::Easing::parseStepPosition("both"), haylen::math::Easing::StepPosition::Both);
    EXPECT_FALSE(haylen::math::Easing::parseStepPosition("middle").has_value());
}

TEST(RandomTest, IsDeterministicPerSeed) {
    haylen::math::Random first(42);
    haylen::math::Random second(42);
    for (int index = 0; index < 100; ++index) {
        EXPECT_EQ(first.nextU64(), second.nextU64());
    }

    second.reseed(7);
    haylen::math::Random third(7);
    EXPECT_EQ(second.nextU32(), third.nextU32());
}

TEST(RandomTest, RangesStayInBounds) {
    haylen::math::Random random(1);
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
    haylen::math::Random random(3);
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
    haylen::math::Random random(9);
    std::array<int, 8> values{1, 2, 3, 4, 5, 6, 7, 8};
    random.shuffle(std::span<int>(values));
    std::sort(values.begin(), values.end());
    EXPECT_EQ(values, (std::array<int, 8>{1, 2, 3, 4, 5, 6, 7, 8}));
}

TEST(NoiseTest, IsDeterministicAndBounded) {
    const haylen::math::Noise2D noise(12);
    const haylen::math::Noise2D same(12);
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
    EXPECT_NE(haylen::math::Noise2D(1).simplex(0.3F, 0.7F), haylen::math::Noise2D(2).simplex(0.3F, 0.7F));
}

// The lattice of the noise repeats every 256 units, so a coordinate far beyond the range of an integer still lands on its cell, and one that is not a number gives no number.
TEST(NoiseTest, SamplesCoordinatesOfAnySize) {
    const haylen::math::Noise2D noise(12);
    EXPECT_EQ(noise.perlin(1e10F, 0.5F), noise.perlin(0.0F, 0.5F));
    EXPECT_EQ(noise.perlin(-1e10F, 0.5F), noise.perlin(0.0F, 0.5F));
    EXPECT_TRUE(std::isnan(noise.perlin(std::numeric_limits<float>::quiet_NaN(), 0.5F)));
    EXPECT_TRUE(std::isnan(noise.simplex(std::numeric_limits<float>::infinity(), 0.5F)));
    EXPECT_TRUE(std::isfinite(noise.worley(1e30F, -1e30F).nearest));
}

TEST(EasingTest, StepsOfAProgressThatIsNotANumberAreNotANumber) {
    EXPECT_TRUE(std::isnan(haylen::math::Easing::steps(std::numeric_limits<float>::quiet_NaN(), 4, haylen::math::Easing::StepPosition::End)));
    EXPECT_EQ(haylen::math::Easing::steps(1.0F, std::numeric_limits<int>::max(), haylen::math::Easing::StepPosition::Both), 1.0F);
}

TEST(GeometryTest, CirclesAndSegments) {
    const haylen::math::Circle circle{haylen::math::Vec2{}, 5.0F};
    EXPECT_TRUE(circle.contains(haylen::math::Vec2(3.0F, 4.0F)));
    EXPECT_FALSE(circle.contains(haylen::math::Vec2(4.0F, 4.0F)));
    EXPECT_EQ(circle.getBounds(), (haylen::math::Rect{-5.0F, -5.0F, 10.0F, 10.0F}));
    EXPECT_TRUE(haylen::math::Geometry::intersects(circle, haylen::math::Circle{haylen::math::Vec2(9.0F, 0.0F), 4.0F}));
    EXPECT_FALSE(haylen::math::Geometry::intersects(circle, haylen::math::Circle{haylen::math::Vec2(10.0F, 0.0F), 4.0F}));
    EXPECT_TRUE(haylen::math::Geometry::intersects(circle, haylen::math::Rect{4.0F, -1.0F, 2.0F, 2.0F}));
    EXPECT_FALSE(haylen::math::Geometry::intersects(circle, haylen::math::Rect{6.0F, 6.0F, 2.0F, 2.0F}));

    const haylen::math::Segment horizontal{haylen::math::Vec2(-1.0F, 0.0F), haylen::math::Vec2(1.0F, 0.0F)};
    const haylen::math::Segment vertical{haylen::math::Vec2(0.0F, -1.0F), haylen::math::Vec2(0.0F, 1.0F)};
    EXPECT_EQ(haylen::math::Geometry::intersection(horizontal, vertical), haylen::math::Vec2{});
    EXPECT_FALSE(haylen::math::Geometry::intersection(horizontal, haylen::math::Segment{haylen::math::Vec2(-1.0F, 1.0F), haylen::math::Vec2(1.0F, 1.0F)}).has_value());
    EXPECT_FALSE(haylen::math::Geometry::intersection(horizontal, haylen::math::Segment{haylen::math::Vec2(2.0F, -1.0F), haylen::math::Vec2(2.0F, 1.0F)}).has_value());
    EXPECT_EQ(haylen::math::Geometry::closestPoint(horizontal, haylen::math::Vec2(5.0F, 3.0F)), haylen::math::Vec2(1.0F, 0.0F));
    EXPECT_EQ(haylen::math::Geometry::closestPoint(haylen::math::Segment{haylen::math::Vec2(1.0F, 1.0F), haylen::math::Vec2(1.0F, 1.0F)}, haylen::math::Vec2{}), haylen::math::Vec2(1.0F, 1.0F));
    EXPECT_FLOAT_EQ(haylen::math::Geometry::distanceToSegment(horizontal, haylen::math::Vec2(0.0F, 2.0F)), 2.0F);
}

TEST(GeometryTest, Polygons) {
    const std::array<haylen::math::Vec2, 4> square{haylen::math::Vec2(0.0F, 0.0F), haylen::math::Vec2(10.0F, 0.0F), haylen::math::Vec2(10.0F, 10.0F), haylen::math::Vec2(0.0F, 10.0F)};
    EXPECT_TRUE(haylen::math::Geometry::contains(square, haylen::math::Vec2(5.0F, 5.0F)));
    EXPECT_FALSE(haylen::math::Geometry::contains(square, haylen::math::Vec2(15.0F, 5.0F)));
    EXPECT_FLOAT_EQ(haylen::math::Geometry::signedArea(square), 100.0F);
    EXPECT_EQ(haylen::math::Geometry::centroid(square), haylen::math::Vec2(5.0F, 5.0F));
    EXPECT_TRUE(haylen::math::Geometry::isConvex(square));
    EXPECT_EQ(haylen::math::Geometry::bounds(square), (haylen::math::Rect{0.0F, 0.0F, 10.0F, 10.0F}));
    EXPECT_EQ(haylen::math::Geometry::bounds(std::span<const haylen::math::Vec2>{}), haylen::math::Rect{});

    const std::array<haylen::math::Vec2, 5> arrow{haylen::math::Vec2(0.0F, 0.0F), haylen::math::Vec2(10.0F, 0.0F), haylen::math::Vec2(5.0F, 3.0F), haylen::math::Vec2(10.0F, 10.0F), haylen::math::Vec2(0.0F, 10.0F)};
    EXPECT_FALSE(haylen::math::Geometry::isConvex(arrow));
    EXPECT_FALSE(haylen::math::Geometry::isConvex(std::span<const haylen::math::Vec2>(square.data(), 2)));

    const std::array<haylen::math::Vec2, 3> line{haylen::math::Vec2(0.0F, 0.0F), haylen::math::Vec2(1.0F, 1.0F), haylen::math::Vec2(2.0F, 2.0F)};
    EXPECT_EQ(haylen::math::Geometry::centroid(line), haylen::math::Vec2(1.0F, 1.0F));
    EXPECT_EQ(haylen::math::Geometry::centroid(std::span<const haylen::math::Vec2>{}), haylen::math::Vec2{});
}

TEST(GeometryTest, StarsThatTurnOneWayAreNotConvex) {
    std::vector<haylen::math::Vec2> hexagon;
    std::vector<haylen::math::Vec2> pentagram;

    for (int index = 0; index < 6; ++index) {
        hexagon.push_back(haylen::math::Vec2::fromAngle(haylen::math::Math::kTau * static_cast<float>(index) / 6.0F, 10.0F));
    }

    for (int index = 0; index < 5; ++index) {
        pentagram.push_back(haylen::math::Vec2::fromAngle(haylen::math::Math::kTau * static_cast<float>(index * 2) / 5.0F, 10.0F));
    }

    EXPECT_TRUE(haylen::math::Geometry::isConvex(hexagon));
    EXPECT_FALSE(haylen::math::Geometry::isConvex(pentagram));
    std::reverse(hexagon.begin(), hexagon.end());
    std::reverse(pentagram.begin(), pentagram.end());
    EXPECT_TRUE(haylen::math::Geometry::isConvex(hexagon));
    EXPECT_FALSE(haylen::math::Geometry::isConvex(pentagram));
}

TEST(GeometryTest, ConvexHullDropsInteriorPoints) {
    const std::array<haylen::math::Vec2, 6> points{haylen::math::Vec2(0.0F, 0.0F), haylen::math::Vec2(4.0F, 0.0F), haylen::math::Vec2(4.0F, 4.0F), haylen::math::Vec2(0.0F, 4.0F), haylen::math::Vec2(2.0F, 2.0F), haylen::math::Vec2(4.0F, 0.0F)};
    const std::vector<haylen::math::Vec2> hull = haylen::math::Geometry::convexHull(points);
    EXPECT_EQ(hull.size(), 4U);
    EXPECT_GT(haylen::math::Geometry::signedArea(hull), 0.0F);
    EXPECT_EQ(haylen::math::Geometry::convexHull(std::span<const haylen::math::Vec2>(points.data(), 2)).size(), 2U);

    // A point with a NaN coordinate is left out instead of breaking the sort.
    const std::array<haylen::math::Vec2, 5> withNaN{haylen::math::Vec2(0.0F, 0.0F), haylen::math::Vec2(4.0F, 0.0F), haylen::math::Vec2(std::numeric_limits<float>::quiet_NaN(), 2.0F), haylen::math::Vec2(4.0F, 4.0F), haylen::math::Vec2(0.0F, 4.0F)};
    EXPECT_EQ(haylen::math::Geometry::convexHull(withNaN).size(), 4U);
}

TEST(GeometryTest, TriangulatesConcavePolygonsInEitherWinding) {
    std::vector<haylen::math::Vec2> arrow{haylen::math::Vec2(0.0F, 0.0F), haylen::math::Vec2(10.0F, 0.0F), haylen::math::Vec2(5.0F, 3.0F), haylen::math::Vec2(10.0F, 10.0F), haylen::math::Vec2(0.0F, 10.0F)};
    const auto triangles = haylen::math::Geometry::triangulate(arrow);
    ASSERT_EQ(triangles.size(), 9U);

    float area = 0.0F;
    for (std::size_t index = 0; index < triangles.size(); index += 3) {
        const std::array<haylen::math::Vec2, 3> triangle{arrow[triangles[index]], arrow[triangles[index + 1]], arrow[triangles[index + 2]]};
        EXPECT_GT(haylen::math::Geometry::signedArea(triangle), 0.0F);
        area += haylen::math::Geometry::signedArea(triangle);
    }

    EXPECT_NEAR(area, haylen::math::Geometry::signedArea(arrow), 1e-3F);

    std::reverse(arrow.begin(), arrow.end());
    EXPECT_EQ(haylen::math::Geometry::triangulate(arrow).size(), 9U);
    EXPECT_TRUE(haylen::math::Geometry::triangulate(std::span<const haylen::math::Vec2>(arrow.data(), 2)).empty());
}

TEST(PoissonDiskTest, RefusesAnAreaWithEdgesThatAreNotFinite) {
    haylen::math::Random random(5);
    const float infinity = std::numeric_limits<float>::infinity();

    for (const haylen::math::Rect area : {haylen::math::Rect{-infinity, 0.0F, 100.0F, 100.0F}, haylen::math::Rect{0.0F, 0.0F, 100.0F, std::numeric_limits<float>::quiet_NaN()}}) {
        try {
            (void)haylen::math::PoissonDisk::sample({.area = area, .minimumDistance = 10.0F}, random);
            ADD_FAILURE() << "An area without finite edges must be refused.";
        } catch (const std::invalid_argument& error) {
            EXPECT_STREQ(error.what(), "Poisson sampling needs an area whose edges are finite.");
        }
    }
}

TEST(PoissonDiskTest, KeepsMinimumDistanceAndPredicate) {
    haylen::math::Random random(5);
    const haylen::math::PoissonDisk::Options options{
        .area = haylen::math::Rect{0.0F, 0.0F, 500.0F, 300.0F},
        .minimumDistance = 40.0F,
        .accept = [](haylen::math::Vec2 point) { return point.x < 400.0F; },
    };
    const std::vector<haylen::math::Vec2> points = haylen::math::PoissonDisk::sample(options, random);

    ASSERT_GT(points.size(), 20U);

    for (std::size_t first = 0; first < points.size(); ++first) {
        EXPECT_LT(points[first].x, 400.0F);
        EXPECT_TRUE(options.area.contains(points[first]));

        for (std::size_t second = first + 1; second < points.size(); ++second) {
            EXPECT_GE(haylen::math::Vec2::distance(points[first], points[second]), 40.0F - 1e-3F);
        }
    }
}

TEST(PoissonDiskTest, RejectsDegenerateInput) {
    haylen::math::Random random(5);
    EXPECT_TRUE(haylen::math::PoissonDisk::sample(haylen::math::PoissonDisk::Options{.area = haylen::math::Rect{}}, random).empty());
    EXPECT_TRUE(haylen::math::PoissonDisk::sample(haylen::math::PoissonDisk::Options{.area = haylen::math::Rect{0.0F, 0.0F, 10.0F, 10.0F}, .minimumDistance = 0.0F}, random).empty());

    const haylen::math::PoissonDisk::Options nothingAccepted{.area = haylen::math::Rect{0.0F, 0.0F, 10.0F, 10.0F}, .accept = [](haylen::math::Vec2) { return false; }};
    EXPECT_TRUE(haylen::math::PoissonDisk::sample(nothingAccepted, random).empty());
    EXPECT_THROW((void)haylen::math::PoissonDisk::sample(haylen::math::PoissonDisk::Options{.area = haylen::math::Rect{0.0F, 0.0F, 2000.0F, 2000.0F}, .minimumDistance = 0.01F}, random), std::invalid_argument);
}

TEST(PoissonDiskTest, BoundsHugeDistancesAndRejectsOnesThatAreNotFinite) {
    haylen::math::Random random(5);
    const haylen::math::Rect area{0.0F, 0.0F, 100.0F, 100.0F};

    // The largest distances reach far beyond any `int` number of cells, and the search stays within the grid.
    for (const float maximum : {1e30F, std::numeric_limits<float>::max()}) {
        const std::vector<haylen::math::Vec2> points = haylen::math::PoissonDisk::sample(haylen::math::PoissonDisk::Options{.area = area, .minimumDistance = 10.0F, .maximumDistance = maximum, .distance = [](haylen::math::Vec2) { return 10.0F; }}, random);
        ASSERT_GT(points.size(), 10U);

        for (std::size_t first = 0; first < points.size(); ++first) {
            for (std::size_t second = first + 1; second < points.size(); ++second) {
                EXPECT_GE(haylen::math::Vec2::distance(points[first], points[second]), 10.0F - 1e-3F);
            }
        }
    }

    const float infinity = std::numeric_limits<float>::infinity();
    EXPECT_THROW((void)haylen::math::PoissonDisk::sample(haylen::math::PoissonDisk::Options{.area = area, .minimumDistance = 10.0F, .maximumDistance = infinity, .distance = [](haylen::math::Vec2) { return 10.0F; }}, random), std::invalid_argument);
    EXPECT_THROW((void)haylen::math::PoissonDisk::sample(haylen::math::PoissonDisk::Options{.area = area, .minimumDistance = 10.0F, .maximumDistance = 20.0F, .distance = [](haylen::math::Vec2) { return std::numeric_limits<float>::quiet_NaN(); }}, random), std::invalid_argument);
}

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
