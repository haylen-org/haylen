#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "haylen/math/Geometry.hpp"
#include "haylen/math/MarchingSquares.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Polygon.hpp"
#include "haylen/math/Spline.hpp"

namespace haylen::math {

class PolygonTest : public ::testing::Test {
  protected:
    static Polygon::Outline square(float x, float y, float size) {
        return {{x, y}, {x + size, y}, {x + size, y + size}, {x, y + size}};
    }

    static Polygon::Outline circle(Vec2 center, float radius, int segments) {
        Polygon::Outline outline;
        for (int index = 0; index < segments; ++index) {
            outline.push_back(center + Vec2::fromAngle(Math::kTau * static_cast<float>(index) / static_cast<float>(segments), radius));
        }
        return outline;
    }

    static float area(const std::vector<Polygon::Outline>& shape) {
        float total = 0.0F;
        for (const Polygon::Outline& outline : shape) {
            total += Geometry::signedArea(outline);
        }
        return total;
    }
};

TEST_F(PolygonTest, BooleanAreasFollowInclusionExclusion) {
    const std::vector<Polygon::Outline> first{square(0.0F, 0.0F, 10.0F)};
    const std::vector<Polygon::Outline> second{square(5.0F, 5.0F, 10.0F)};

    EXPECT_NEAR(area(Polygon::unite(first, second)), 175.0F, 1e-3F);
    EXPECT_NEAR(area(Polygon::intersect(first, second)), 25.0F, 1e-3F);
    EXPECT_NEAR(area(Polygon::subtract(first, second)), 75.0F, 1e-3F);
    EXPECT_NEAR(area(Polygon::exclude(first, second)), 150.0F, 1e-3F);
    EXPECT_NEAR(Polygon::getArea(first), 100.0F, 1e-3F);
}

TEST_F(PolygonTest, ResultsWindOuterOutlinesPositiveAndHolesNegative) {
    const std::vector<Polygon::Outline> outer{square(0.0F, 0.0F, 30.0F)};
    const std::vector<Polygon::Outline> inner{square(10.0F, 10.0F, 10.0F)};

    const std::vector<Polygon::Outline> ring = Polygon::subtract(outer, inner);
    ASSERT_EQ(ring.size(), 2U);
    EXPECT_EQ(std::count_if(ring.begin(), ring.end(), [](const Polygon::Outline& outline) { return Geometry::signedArea(outline) > 0.0F; }), 1);
    EXPECT_NEAR(area(ring), 800.0F, 1e-3F);

    // A clockwise input fills the same area as a counter-clockwise one.
    Polygon::Outline reversed = square(0.0F, 0.0F, 10.0F);
    std::reverse(reversed.begin(), reversed.end());
    EXPECT_NEAR(Polygon::getArea(std::vector<Polygon::Outline>{reversed}), 100.0F, 1e-3F);
}

TEST_F(PolygonTest, OffsetGrowsAndShrinks) {
    const std::vector<Polygon::Outline> shape{square(0.0F, 0.0F, 10.0F)};

    EXPECT_NEAR(area(Polygon::offset(shape, 1.0F, Polygon::Join::Miter)), 144.0F, 1e-2F);
    EXPECT_NEAR(area(Polygon::offset(shape, -1.0F, Polygon::Join::Miter)), 64.0F, 1e-2F);
    // Round corners add a quarter circle at each corner instead of a square.
    EXPECT_NEAR(area(Polygon::offset(shape, 1.0F)), 140.0F + Math::kPi, 0.05F);
    EXPECT_TRUE(Polygon::offset(shape, -6.0F).empty());
}

TEST_F(PolygonTest, SimplifyDropsPointsWithinTheTolerance) {
    const std::vector<Vec2> line{{0.0F, 0.0F}, {1.0F, 0.05F}, {2.0F, -0.05F}, {3.0F, 0.0F}, {3.0F, 3.0F}};
    const Polygon::Outline simplified = Polygon::simplify(line, 0.1F, false);
    EXPECT_EQ(simplified, (Polygon::Outline{{0.0F, 0.0F}, {3.0F, 0.0F}, {3.0F, 3.0F}}));
    EXPECT_EQ(Polygon::simplify(line, 0.01F, false).size(), line.size());

    Polygon::Outline dense;
    for (int index = 0; index < 40; ++index) {
        dense.push_back({static_cast<float>(index), 0.0F});
    }
    for (int index = 0; index < 40; ++index) {
        dense.push_back({40.0F, static_cast<float>(index)});
    }
    dense.push_back({0.0F, 40.0F});
    const Polygon::Outline corners = Polygon::simplify(dense, 0.5F);
    EXPECT_EQ(corners.size(), 4U);
    EXPECT_NEAR(std::fabs(Geometry::signedArea(corners)), 1580.0F, 1e-3F);

    // An outline thinner than the tolerance still keeps three points.
    EXPECT_EQ(Polygon::simplify(std::vector<Vec2>{{0.0F, 0.0F}, {10.0F, 0.1F}, {20.0F, 0.0F}, {10.0F, -0.1F}}, 1.0F).size(), 3U);
}

TEST_F(PolygonTest, DecompositionCoversTheShapeWithConvexPieces) {
    // An L shape with a notch and a hole.
    const Polygon::Outline outline{{0.0F, 0.0F}, {60.0F, 0.0F}, {60.0F, 20.0F}, {30.0F, 25.0F}, {20.0F, 60.0F}, {0.0F, 60.0F}};
    Polygon::Outline hole = square(5.0F, 5.0F, 8.0F);
    std::reverse(hole.begin(), hole.end());
    const std::vector<Polygon::Outline> shape{outline, hole};

    const std::vector<Polygon::Outline> pieces = Polygon::decompose(shape);
    ASSERT_FALSE(pieces.empty());
    float total = 0.0F;
    for (const Polygon::Outline& piece : pieces) {
        EXPECT_TRUE(Geometry::isConvex(piece));
        EXPECT_LE(piece.size(), 8U);
        EXPECT_GT(Geometry::signedArea(piece), 0.0F);
        total += Geometry::signedArea(piece);
    }
    EXPECT_NEAR(total, Polygon::getArea(shape), 0.05F);
    EXPECT_NEAR(area(Polygon::unite(pieces)), Polygon::getArea(shape), 0.05F);
    EXPECT_LT(pieces.size(), Polygon::decompose(shape, 3).size());

    EXPECT_EQ(Polygon::decompose(std::vector<Polygon::Outline>{square(0.0F, 0.0F, 4.0F)}).size(), 1U);
    EXPECT_THROW((void)Polygon::decompose(shape, 2), std::invalid_argument);
}

TEST(MarchingSquaresTest, TracesClosedOutlinesAroundAField) {
    // A 3 by 3 block of ones in a 7 by 7 field.
    std::vector<float> field(49, 0.0F);
    for (int y = 2; y <= 4; ++y) {
        for (int x = 2; x <= 4; ++x) {
            field[static_cast<std::size_t>(y * 7 + x)] = 1.0F;
        }
    }
    const std::vector<std::vector<Vec2>> outlines = MarchingSquares::trace(field, 7, 7, {.threshold = 0.5F, .spacing = 2.0F, .origin = {10.0F, 0.0F}});
    ASSERT_EQ(outlines.size(), 1U);
    EXPECT_GT(Geometry::signedArea(outlines[0]), 0.0F);
    // The outline runs halfway between inside and outside samples, cutting the corners diagonally.
    EXPECT_NEAR(Geometry::signedArea(outlines[0]), (9.0F - 0.5F) * 4.0F, 1e-3F);
    const Rect bounds = Geometry::bounds(outlines[0]);
    EXPECT_FLOAT_EQ(bounds.x, 13.0F);
    EXPECT_FLOAT_EQ(bounds.width, 6.0F);

    EXPECT_THROW((void)MarchingSquares::trace(field, 6, 7), std::invalid_argument);

    // An empty grid traces nothing, however long its other side is.
    EXPECT_TRUE(MarchingSquares::trace({}, 0, 100000000).empty());
    EXPECT_TRUE(MarchingSquares::traceBitmap({}, 100000000, 0).empty());
}

TEST(MarchingSquaresTest, TracesHolesAndSeparateIslandsOfABitmap) {
    // A ring with a hole in the middle and a separate pixel.
    const std::vector<std::uint8_t> pixels{
        1, 1, 1, 0, 0, //
        1, 0, 1, 0, 1, //
        1, 1, 1, 0, 0, //
    };
    const std::vector<std::vector<Vec2>> outlines = MarchingSquares::traceBitmap(pixels, 5, 3);
    ASSERT_EQ(outlines.size(), 3U);

    int holes = 0;
    float total = 0.0F;
    for (const std::vector<Vec2>& outline : outlines) {
        holes += Geometry::signedArea(outline) < 0.0F ? 1 : 0;
        total += Geometry::signedArea(outline);
        // Consecutive points never repeat, and the last point does not repeat the first.
        for (std::size_t index = 0; index < outline.size(); ++index) {
            EXPECT_NE(outline[index], outline[(index + 1) % outline.size()]);
        }
    }
    EXPECT_EQ(holes, 1);
    // Outlines cut an eighth of a pixel from every outer corner, so the ring covers 9 - 0.5 around a hole of 0.5, and the lone pixel covers 0.5.
    EXPECT_NEAR(total, 8.5F, 1e-4F);
}

TEST(MarchingSquaresTest, ClosesAreasAlongTheEdgeOfTheGrid) {
    const std::vector<float> full(16, 1.0F);
    const std::vector<std::vector<Vec2>> outlines = MarchingSquares::trace(full, 4, 4);
    ASSERT_EQ(outlines.size(), 1U);
    EXPECT_NEAR(Geometry::signedArea(outlines[0]), 9.0F, 1e-4F);
    EXPECT_TRUE(MarchingSquares::trace(std::vector<float>(16, 0.0F), 4, 4).empty());
}

TEST(SplineTest, CatmullRomPassesThroughItsPoints) {
    const Spline spline({{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}});
    EXPECT_EQ(spline.getSegmentCount(), 3U);
    EXPECT_EQ(spline.getPoint(0.0F), Vec2(0.0F, 0.0F));
    EXPECT_NEAR(spline.getPoint(1.0F / 3.0F).x, 100.0F, 1e-3F);
    EXPECT_NEAR(spline.getPoint(1.0F).y, 100.0F, 1e-3F);
    EXPECT_GT(spline.getLength(), 300.0F);

    const Spline loop({{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}}, Spline::Kind::CatmullRom, true);
    EXPECT_EQ(loop.getSegmentCount(), 4U);
    EXPECT_NEAR(Vec2::distance(loop.getPoint(1.0F), loop.getPoint(0.0F)), 0.0F, 1e-3F);
}

TEST(SplineTest, SamplesEvenlyByDistance) {
    const Spline curve({{0.0F, 0.0F}, {50.0F, 80.0F}, {150.0F, -40.0F}, {200.0F, 30.0F}}, Spline::Kind::Bezier);
    const std::vector<Vec2> points = curve.sampleByDistance(10.0F);
    ASSERT_GT(points.size(), 10U);
    for (std::size_t index = 1; index + 1 < points.size(); ++index) {
        EXPECT_NEAR(Vec2::distance(points[index - 1], points[index]), 10.0F, 0.2F);
    }
    EXPECT_EQ(points.back(), curve.getPoint(1.0F));
    EXPECT_EQ(curve.getPoint(0.0F), Vec2(0.0F, 0.0F));
    EXPECT_NEAR(curve.getPoint(1.0F).x, 200.0F, 1e-3F);

    const Vec2 middle = curve.getPointAtDistance(curve.getLength() * 0.5F);
    EXPECT_EQ(middle, curve.getPoint(curve.getParameterAtDistance(curve.getLength() * 0.5F)));
    EXPECT_NEAR(curve.getTangentAtDistance(0.0F).getLength(), 1.0F, 1e-4F);
}

TEST(SplineTest, BSplineReachesItsEndsAndSmoothsBetween) {
    const Spline spline({{0.0F, 0.0F}, {10.0F, 10.0F}, {20.0F, 0.0F}}, Spline::Kind::BSpline);
    EXPECT_EQ(spline.getSegmentCount(), 4U);
    EXPECT_NEAR(Vec2::distance(spline.getPoint(0.0F), {0.0F, 0.0F}), 0.0F, 1e-4F);
    EXPECT_NEAR(Vec2::distance(spline.getPoint(1.0F), {20.0F, 0.0F}), 0.0F, 1e-4F);
    // The curve stays below the middle control point instead of passing through it.
    EXPECT_LT(spline.getPoint(0.5F).y, 10.0F);
    EXPECT_EQ(spline.sample(5).size(), 5U);
}

TEST(SplineTest, RejectsWrongPointCounts) {
    EXPECT_THROW(Spline({{0.0F, 0.0F}}), std::invalid_argument);
    EXPECT_THROW(Spline({{0.0F, 0.0F}, {1.0F, 0.0F}, {2.0F, 0.0F}}, Spline::Kind::Bezier), std::invalid_argument);
    EXPECT_THROW(Spline({{0.0F, 0.0F}, {1.0F, 0.0F}}, Spline::Kind::BSpline, true), std::invalid_argument);
    EXPECT_THROW((void)Spline({{0.0F, 0.0F}, {1.0F, 0.0F}}).sampleByDistance(0.0F), std::invalid_argument);
    EXPECT_EQ(Spline::kindFromName("bSpline"), Spline::Kind::BSpline);
    EXPECT_EQ(Spline::kindName(Spline::Kind::CatmullRom), "catmullRom");
}

} // namespace haylen::math
