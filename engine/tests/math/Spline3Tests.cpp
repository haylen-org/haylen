#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include "haylen/math/CurveFrame.hpp"
#include "haylen/math/CurvePath3.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/NurbsSurface.hpp"
#include "haylen/math/NurbsVolume.hpp"
#include "haylen/math/Spline2.hpp"
#include "haylen/math/Spline3.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

class Spline3Test : public ::testing::Test {
  protected:
    static constexpr float kStep = 1e-3F;

    static std::vector<Vec3> lift(const std::vector<Vec2>& points) {
        std::vector<Vec3> lifted;
        for (const Vec2 point : points) {
            lifted.push_back({point.x, point.y, 0.0F});
        }

        return lifted;
    }

    // Compares the curve with the 2D spline of the same points in the plane where z is zero, which evaluates every kind its own way.
    static void expectSameAsFlat(const std::vector<Vec2>& points, Spline2::Kind flatKind, Spline3::Kind kind, bool closed) {
        const Spline2 flat(points, flatKind, closed);
        const Spline3 curve(lift(points), {.kind = kind, .closed = closed});
        EXPECT_EQ(curve.getSegmentCount(), flat.getSegmentCount());
        for (int step = 0; step <= 100; ++step) {
            const float t = static_cast<float>(step) / 100.0F;
            const Vec2 expected = flat.getPoint(t);
            const Vec3 actual = curve.getPoint(t);
            EXPECT_NEAR(actual.x, expected.x, 1e-3F) << t;
            EXPECT_NEAR(actual.y, expected.y, 1e-3F) << t;
            EXPECT_EQ(actual.z, 0.0F);
        }

        EXPECT_NEAR(curve.getLength(), flat.getLength(), flat.getLength() * 1e-4F);
    }

    static void expectNear(Vec3 actual, Vec3 expected, float tolerance) {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }

    // The quarter of the unit circle in the plane of x and y, the conic that a rational curve of degree 2 draws exactly.
    static Spline3 quarterCircle() {
        return Spline3({{1.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}}, {.kind = Spline3::Kind::Nurbs, .weights = {1.0F, std::sqrt(0.5F), 1.0F}, .knots = {0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F}, .degree = 2});
    }
};

TEST_F(Spline3Test, MatchesTheFlatSplineOfEveryKind) {
    const std::vector<Vec2> points{{0.0F, 0.0F}, {40.0F, 80.0F}, {120.0F, -20.0F}, {160.0F, 40.0F}, {200.0F, 10.0F}, {260.0F, 90.0F}, {300.0F, 0.0F}};
    expectSameAsFlat(points, Spline2::Kind::CatmullRom, Spline3::Kind::CatmullRom, false);
    expectSameAsFlat(points, Spline2::Kind::CatmullRom, Spline3::Kind::CatmullRom, true);
    expectSameAsFlat(points, Spline2::Kind::Bezier, Spline3::Kind::Bezier, false);
    expectSameAsFlat({points.begin(), points.begin() + 6}, Spline2::Kind::Bezier, Spline3::Kind::Bezier, true);
    expectSameAsFlat(points, Spline2::Kind::BSpline, Spline3::Kind::BSpline, false);
    expectSameAsFlat(points, Spline2::Kind::BSpline, Spline3::Kind::BSpline, true);
}

// A Catmull-Rom curve passes through its points, and the curve and its direction meet without a jump at every point where two segments join, where a B-spline keeps its velocity too.
TEST_F(Spline3Test, PassesThroughItsPointsAndJoinsSmoothly) {
    const std::vector<Vec3> points{{0.0F, 0.0F, 0.0F}, {2.0F, 3.0F, -1.0F}, {5.0F, 1.0F, 2.0F}, {7.0F, 4.0F, 0.0F}, {9.0F, 0.0F, 3.0F}};
    for (const Spline3::Kind kind : {Spline3::Kind::CatmullRom, Spline3::Kind::BSpline}) {
        const Spline3 curve(points, {.kind = kind});
        const auto segments = static_cast<float>(curve.getSegmentCount());
        for (std::size_t join = 1; join < curve.getSegmentCount(); ++join) {
            const float t = static_cast<float>(join) / segments;
            const Spline3::Derivatives before = curve.getDerivatives(t - 1e-4F);
            const Spline3::Derivatives after = curve.getDerivatives(t + 1e-4F);
            expectNear(before.point, after.point, 1e-2F);
            EXPECT_GT(Vec3::dot(before.first.getNormalized(), after.first.getNormalized()), 0.999F) << join;
            if (kind == Spline3::Kind::BSpline) {
                expectNear(before.first, after.first, before.first.getLength() * 1e-2F);
            }

            if (kind == Spline3::Kind::CatmullRom) {
                expectNear(curve.getPoint(t), points[join], 1e-4F);
            }
        }
    }

    const Spline3 open(points);
    expectNear(open.getPoint(0.0F), points.front(), 1e-6F);
    expectNear(open.getPoint(1.0F), points.back(), 1e-5F);
    const Spline3 bSpline(points, {.kind = Spline3::Kind::BSpline});
    expectNear(bSpline.getPoint(0.0F), points.front(), 1e-6F);
    expectNear(bSpline.getPoint(1.0F), points.back(), 1e-5F);

    // A closed curve ends where it starts, in the same direction.
    const Spline3 loop(points, {.closed = true});
    expectNear(loop.getPoint(1.0F), loop.getPoint(0.0F), 1e-5F);
    EXPECT_GT(Vec3::dot(loop.getTangent(0.9999F), loop.getTangent(0.0001F)), 0.999F);
}

TEST_F(Spline3Test, DerivativesMatchTheChangeOfThePoint) {
    const Spline3 curve({{0.0F, 0.0F, 0.0F}, {1.0F, 2.0F, 0.5F}, {3.0F, 2.5F, -1.0F}, {4.0F, 0.0F, 0.0F}, {6.0F, 1.0F, 2.0F}, {7.0F, 3.0F, 1.0F}, {9.0F, 0.0F, 0.0F}}, {.kind = Spline3::Kind::Bezier});
    for (const float t : {0.1F, 0.3F, 0.45F, 0.7F, 0.9F}) {
        const Spline3::Derivatives at = curve.getDerivatives(t);
        const Vec3 velocity = (curve.getPoint(t + kStep) - curve.getPoint(t - kStep)) / (2.0F * kStep);
        const Vec3 acceleration = (curve.getDerivatives(t + kStep).first - curve.getDerivatives(t - kStep).first) / (2.0F * kStep);
        expectNear(at.first, velocity, velocity.getLength() * 1e-2F);
        expectNear(at.second, acceleration, acceleration.getLength() * 2e-2F + 0.5F);
    }

    const Vec3 tangent = curve.getTangent(0.3F);
    EXPECT_NEAR(tangent.getLength(), 1.0F, 1e-5F);
    EXPECT_GT(Vec3::dot(tangent, curve.getDerivatives(0.3F).first), 0.0F);
}

// Weights pull a NURBS curve toward their points, so the right weights draw a circle that no polynomial curve can.
TEST_F(Spline3Test, DrawsExactConicsWithWeightsAndKnots) {
    const Spline3 arc = quarterCircle();
    EXPECT_EQ(arc.getKind(), Spline3::Kind::Nurbs);
    EXPECT_EQ(arc.getDegree(), 2);
    EXPECT_EQ(arc.getSegmentCount(), 1U);
    for (int step = 0; step <= 50; ++step) {
        EXPECT_NEAR(arc.getPoint(static_cast<float>(step) / 50.0F).getLength(), 1.0F, 1e-5F) << step;
        EXPECT_NEAR(Vec3::dot(arc.getPoint(static_cast<float>(step) / 50.0F), arc.getTangent(static_cast<float>(step) / 50.0F)), 0.0F, 1e-4F) << step;
    }

    EXPECT_NEAR(arc.getLength(), Math::kHalfPi, 1e-3F);

    // Clamped uniform knots come from the points when none are given, so the curve reaches both ends.
    const Spline3 uniform({{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 0.0F}, {2.0F, 0.0F, 0.0F}, {3.0F, 1.0F, 0.0F}, {4.0F, 0.0F, 0.0F}}, {.kind = Spline3::Kind::Nurbs});
    EXPECT_EQ(uniform.getKnots(), (std::vector<float>{0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 2.0F, 2.0F, 2.0F, 2.0F}));
    EXPECT_EQ(uniform.getSegmentCount(), 2U);
    expectNear(uniform.getPoint(0.0F), {0.0F, 0.0F, 0.0F}, 1e-6F);
    expectNear(uniform.getPoint(1.0F), {4.0F, 0.0F, 0.0F}, 1e-5F);

    // A straight line of degree 1 has its length exactly and walks at an even speed by distance.
    const Spline3 line({{0.0F, 0.0F, 0.0F}, {3.0F, 4.0F, 0.0F}, {3.0F, 4.0F, 12.0F}}, {.kind = Spline3::Kind::Nurbs, .degree = 1});
    EXPECT_NEAR(line.getLength(), 17.0F, 1e-3F);
    expectNear(line.getPointAtDistance(5.0F), {3.0F, 4.0F, 0.0F}, 1e-3F);
    expectNear(line.getPointAtDistance(11.0F), {3.0F, 4.0F, 6.0F}, 1e-3F);
    expectNear(line.getTangentAtDistance(11.0F), {0.0F, 0.0F, 1.0F}, 1e-5F);
}

TEST_F(Spline3Test, FindsTheClosestPointAtARightAngle) {
    const Spline3 arc = quarterCircle();
    const Spline3::ClosestPoint closest = arc.getClosestPoint({2.0F, 2.0F, 0.0F});
    expectNear(closest.point, {std::sqrt(0.5F), std::sqrt(0.5F), 0.0F}, 1e-4F);
    EXPECT_NEAR(closest.distance, 2.0F * std::sqrt(2.0F) - 1.0F, 1e-4F);
    EXPECT_NEAR(closest.t, 0.5F, 1e-3F);

    // A point past the end of an open curve is closest to that end, and a point on the curve lies at no distance.
    const Spline3::ClosestPoint end = arc.getClosestPoint({3.0F, -2.0F, 0.0F});
    EXPECT_NEAR(end.t, 0.0F, 1e-6F);
    expectNear(end.point, {1.0F, 0.0F, 0.0F}, 1e-5F);
    EXPECT_NEAR(arc.getClosestPoint(arc.getPoint(0.3F)).distance, 0.0F, 1e-4F);

    const Spline3 track({{0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}, {10.0F, 0.0F, -10.0F}, {0.0F, 0.0F, -10.0F}}, {.closed = true});
    const Spline3::ClosestPoint aboveTrack = track.getClosestPoint({5.0F, 3.0F, 0.5F});
    EXPECT_NEAR(Vec3::dot(aboveTrack.point - Vec3{5.0F, 3.0F, 0.5F}, track.getTangent(aboveTrack.t)), 0.0F, 1e-2F);
}

// Parallel transport keeps the frames at right angles without spinning around the tangent, so a straight piece keeps its normal and a closed loop meets its first frame.
TEST_F(Spline3Test, CarriesFramesWithoutTwist) {
    const Spline3 straight({{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -10.0F}});
    for (const CurveFrame& frame : straight.getFrames(5)) {
        expectNear(frame.tangent, Vec3::forward(), 1e-5F);
        expectNear(frame.normal, Vec3::up(), 1e-5F);
        expectNear(frame.binormal, Vec3::cross(Vec3::forward(), Vec3::up()), 1e-5F);
    }

    // A vertical piece runs along the up, so its first normal crosses the tangent along the axis of the world most across it.
    const std::vector<CurveFrame> rising = Spline3({{0.0F, 0.0F, 0.0F}, {0.0F, 5.0F, 0.0F}}).getFrames(2);
    EXPECT_NEAR(Vec3::dot(rising.front().normal, Vec3::up()), 0.0F, 1e-6F);

    const Spline3 helix({{1.0F, 0.0F, 0.0F}, {0.0F, 0.5F, -1.0F}, {-1.0F, 1.0F, 0.0F}, {0.0F, 1.5F, 1.0F}, {1.0F, 2.0F, 0.0F}, {0.0F, 2.5F, -1.0F}});
    const std::vector<CurveFrame> frames = helix.getFrames(64);
    ASSERT_EQ(frames.size(), 64U);
    for (const CurveFrame& frame : frames) {
        EXPECT_NEAR(frame.tangent.getLength(), 1.0F, 1e-4F);
        EXPECT_NEAR(frame.normal.getLength(), 1.0F, 1e-4F);
        EXPECT_NEAR(Vec3::dot(frame.tangent, frame.normal), 0.0F, 1e-4F);
        EXPECT_NEAR(Vec3::dot(frame.binormal, frame.normal), 0.0F, 1e-4F);
    }

    for (std::size_t index = 1; index < frames.size(); ++index) {
        EXPECT_GT(Vec3::dot(frames[index].normal, frames[index - 1].normal), 0.9F) << index;
    }

    const Spline3 ring({{4.0F, 0.0F, 0.0F}, {0.0F, 2.0F, -4.0F}, {-4.0F, 0.0F, 0.0F}, {0.0F, -2.0F, 4.0F}}, {.closed = true});
    const std::vector<CurveFrame> around = ring.getFrames(48);
    expectNear(around.back().position, around.front().position, 1e-4F);
    expectNear(around.back().normal, around.front().normal, 1e-3F);
}

// Straight pieces, quadratic segments and the parameterizations and tension of Catmull-Rom curves all pass through the points they should.
TEST_F(Spline3Test, BuildsLinesQuadraticSegmentsAndEveryCatmullRom) {
    const Spline3 line({{0.0F, 0.0F, 0.0F}, {3.0F, 0.0F, 0.0F}, {3.0F, 4.0F, 0.0F}}, {.kind = Spline3::Kind::Linear});
    EXPECT_EQ(line.getDegree(), 1);
    EXPECT_NEAR(line.getLength(), 7.0F, 1e-4F);
    expectNear(line.getPoint(0.5F), {3.0F, 0.0F, 0.0F}, 1e-5F);
    const Spline3 square({{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}}, {.kind = Spline3::Kind::Linear, .closed = true});
    EXPECT_NEAR(square.getLength(), 4.0F, 1e-4F);

    // A quadratic segment from (0, 0) through the control (1, 2) to (2, 0) peaks at a height of 1 halfway.
    const Spline3 arch({{0.0F, 0.0F, 0.0F}, {1.0F, 2.0F, 0.0F}, {2.0F, 0.0F, 0.0F}, {3.0F, -2.0F, 0.0F}, {4.0F, 0.0F, 0.0F}}, {.kind = Spline3::Kind::QuadraticBezier});
    EXPECT_EQ(arch.getDegree(), 2);
    EXPECT_EQ(arch.getSegmentCount(), 2U);
    expectNear(arch.getPoint(0.25F), {1.0F, 1.0F, 0.0F}, 1e-5F);
    expectNear(arch.getPoint(0.5F), {2.0F, 0.0F, 0.0F}, 1e-5F);
    EXPECT_THROW(Spline3({{0.0F, 0.0F, 0.0F}, {1.0F, 2.0F, 0.0F}, {2.0F, 0.0F, 0.0F}, {3.0F, 0.0F, 0.0F}}, {.kind = Spline3::Kind::QuadraticBezier}), std::invalid_argument);

    const std::vector<Vec3> points{{0.0F, 0.0F, 0.0F}, {1.0F, 4.0F, 0.0F}, {6.0F, 4.5F, 1.0F}, {7.0F, 0.0F, 0.0F}};
    for (const float alpha : {0.0F, 0.5F, 1.0F}) {
        const Spline3 curve(points, {.alpha = alpha});
        for (std::size_t index = 0; index < points.size(); ++index) {
            expectNear(curve.getPoint(static_cast<float>(index) / 3.0F), points[index], 1e-4F);
        }
    }

    // A full tension pulls every tangent to nothing, so each segment runs straight between its points.
    const Spline3 taut(points, {.tension = 1.0F});
    expectNear(taut.getPoint(1.0F / 6.0F), (points[0] + points[1]) * 0.5F, 1e-4F);
    EXPECT_GT(Spline3(points, {.alpha = 1.0F}).getLength(), taut.getLength());
    EXPECT_THROW(Spline3(points, {.alpha = 2.0F}), std::invalid_argument);
    EXPECT_THROW(Spline3(points, {.tension = -0.5F}), std::invalid_argument);
    EXPECT_THROW(Spline3(points, {.kind = Spline3::Kind::BSpline, .tension = 0.5F}), std::invalid_argument);

    // Evenly spaced points lie the same distance along the curve from each other.
    const std::vector<Vec3> spaced = Spline3(points).sampleEvenly(5);
    ASSERT_EQ(spaced.size(), 5U);
    expectNear(spaced.front(), points.front(), 1e-5F);
    expectNear(spaced.back(), points.back(), 1e-4F);
}

// A path walks its curves one after another by distance and tells how smoothly each pair joins.
TEST_F(Spline3Test, WalksPathsOfCurvesAndChecksTheirJoins) {
    const Spline3 first({{0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}}, {.kind = Spline3::Kind::Linear});
    const Spline3 second({{10.0F, 0.0F, 0.0F}, {20.0F, 0.0F, 0.0F}}, {.kind = Spline3::Kind::Linear});
    const Spline3 corner({{20.0F, 0.0F, 0.0F}, {20.0F, 0.0F, -5.0F}}, {.kind = Spline3::Kind::Linear});
    const CurvePath3 road({first, second, corner});
    EXPECT_NEAR(road.getLength(), 25.0F, 1e-3F);
    EXPECT_EQ(road.getJoins(), (std::vector<CurvePath3::Continuity>{CurvePath3::Continuity::Velocity, CurvePath3::Continuity::Position}));
    expectNear(road.getPointAtDistance(15.0F), {15.0F, 0.0F, 0.0F}, 1e-3F);
    expectNear(road.getPointAtDistance(22.0F), {20.0F, 0.0F, -2.0F}, 1e-3F);
    expectNear(road.getTangentAtDistance(22.0F), Vec3::forward(), 1e-5F);
    expectNear(road.getPoint(1.0F), {20.0F, 0.0F, -5.0F}, 1e-3F);
    EXPECT_NEAR(road.getClosestPoint({12.0F, 3.0F, 0.0F}).distance, 3.0F, 1e-3F);
    EXPECT_NEAR(road.getClosestPoint({12.0F, 3.0F, 0.0F}).t, 12.0F / 25.0F, 1e-3F);
    EXPECT_EQ(road.sampleByDistance(5.0F).size(), 6U);
    EXPECT_EQ(road.getFrames(6).size(), 6U);

    const Spline3 half({{2.0F, 0.0F, 0.0F}, {2.0F, 0.0F, -2.0F}, {-2.0F, 0.0F, -2.0F}, {-2.0F, 0.0F, 0.0F}}, {.kind = Spline3::Kind::Bezier});
    const Spline3 otherHalf({{-2.0F, 0.0F, 0.0F}, {-2.0F, 0.0F, 2.0F}, {2.0F, 0.0F, 2.0F}, {2.0F, 0.0F, 0.0F}}, {.kind = Spline3::Kind::Bezier});
    const CurvePath3 loop({half, otherHalf}, true);
    EXPECT_EQ(loop.getJoins().size(), 2U);
    EXPECT_EQ(loop.getJoins()[0], CurvePath3::Continuity::Velocity);
    expectNear(loop.getPointAtDistance(loop.getLength() + 0.5F), loop.getPointAtDistance(0.5F), 1e-4F);

    EXPECT_THROW(CurvePath3({}), std::invalid_argument);
    EXPECT_THROW(CurvePath3({first, corner}), std::invalid_argument);
    EXPECT_THROW(CurvePath3({first, second}, true), std::invalid_argument);
    EXPECT_EQ(CurvePath3::continuityFromName("tangent"), CurvePath3::Continuity::Tangent);
}

// A NURBS surface of weights that draw circles makes a quarter of a cylinder, and a volume of degree 1 over a cube maps its parameters to the points of the cube.
TEST_F(Spline3Test, EvaluatesNurbsSurfacesAndVolumes) {
    const float diagonal = std::sqrt(0.5F);
    const std::vector<Vec3> grid{{1.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, -3.0F}, {1.0F, 1.0F, -3.0F}, {0.0F, 1.0F, -3.0F}};
    const NurbsSurface wall(grid, {.columns = 3, .weights = {1.0F, diagonal, 1.0F, 1.0F, diagonal, 1.0F}, .knotsU = {0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F}, .degreeU = 2, .degreeV = 1});
    EXPECT_EQ(wall.getRows(), 2U);
    for (const float u : {0.0F, 0.3F, 0.7F, 1.0F}) {
        for (const float v : {0.0F, 0.5F, 1.0F}) {
            const Vec3 point = wall.getPoint(u, v);
            EXPECT_NEAR(point.x * point.x + point.y * point.y, 1.0F, 1e-5F);
            EXPECT_NEAR(point.z, -3.0F * v, 1e-5F);
            const Vec3 normal = wall.getNormal(u, v);
            EXPECT_NEAR(std::fabs(Vec3::dot(normal, Vec3{point.x, point.y, 0.0F})), 1.0F, 1e-3F);
        }
    }

    const NurbsSurface::Derivatives along = wall.getDerivatives(0.5F, 0.5F);
    expectNear(along.alongV, {0.0F, 0.0F, -3.0F}, 1e-4F);
    EXPECT_THROW(NurbsSurface(grid, {.columns = 4}), std::invalid_argument);
    EXPECT_THROW(NurbsSurface(grid, {.columns = 3}), std::invalid_argument);
    EXPECT_THROW(NurbsSurface(grid, {.columns = 3, .weights = {1.0F}, .degreeU = 2, .degreeV = 1}), std::invalid_argument);

    std::vector<Vec3> cube;
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                cube.push_back({static_cast<float>(x) * 2.0F, static_cast<float>(y) * 3.0F, static_cast<float>(z) * 4.0F});
            }
        }
    }

    const NurbsVolume block(cube, {.columns = 2, .rows = 2, .degreeU = 1, .degreeV = 1, .degreeW = 1});
    EXPECT_EQ(block.getLayers(), 2U);
    expectNear(block.getPoint(0.25F, 0.5F, 0.75F), {0.5F, 1.5F, 3.0F}, 1e-5F);
    EXPECT_THROW(NurbsVolume(cube, {.columns = 3, .rows = 2}), std::invalid_argument);
    EXPECT_THROW(NurbsVolume(cube, {.columns = 2, .rows = 2}), std::invalid_argument);
}

TEST_F(Spline3Test, RefusesCurvesThatDoNotExist) {
    const std::vector<Vec3> two{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}};
    EXPECT_THROW(Spline3({{0.0F, 0.0F, 0.0F}}), std::invalid_argument);
    EXPECT_THROW(Spline3(two, {.closed = true}), std::invalid_argument);
    EXPECT_THROW(Spline3(two, {.kind = Spline3::Kind::Bezier}), std::invalid_argument);
    EXPECT_THROW(Spline3(two, {.kind = Spline3::Kind::BSpline, .closed = true}), std::invalid_argument);
    EXPECT_THROW(Spline3(two, {.weights = {1.0F, 1.0F}}), std::invalid_argument);
    EXPECT_THROW(Spline3(two, {.kind = Spline3::Kind::Nurbs}), std::invalid_argument);
    EXPECT_THROW(Spline3(two, {.kind = Spline3::Kind::Nurbs, .closed = true, .degree = 1}), std::invalid_argument);
    EXPECT_THROW(Spline3(two, {.kind = Spline3::Kind::Nurbs, .degree = 0}), std::invalid_argument);
    EXPECT_THROW(Spline3(two, {.kind = Spline3::Kind::Nurbs, .weights = {1.0F}, .degree = 1}), std::invalid_argument);
    EXPECT_THROW(Spline3(two, {.kind = Spline3::Kind::Nurbs, .weights = {1.0F, 0.0F}, .degree = 1}), std::invalid_argument);
    EXPECT_THROW(Spline3(two, {.kind = Spline3::Kind::Nurbs, .knots = {0.0F, 0.0F, 1.0F}, .degree = 1}), std::invalid_argument);
    EXPECT_THROW(Spline3(two, {.kind = Spline3::Kind::Nurbs, .knots = {0.0F, 1.0F, 0.5F, 1.0F}, .degree = 1}), std::invalid_argument);
    EXPECT_THROW(Spline3(two, {.kind = Spline3::Kind::Nurbs, .knots = {0.0F, 1.0F, 1.0F, 2.0F}, .degree = 1}), std::invalid_argument);

    const std::vector<Vec3> three{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {2.0F, 0.0F, 0.0F}};
    EXPECT_THROW(Spline3(three, {.kind = Spline3::Kind::Nurbs, .knots = {0.0F, 0.0F, 0.5F, 0.5F, 1.0F, 1.0F}, .degree = 1}), std::invalid_argument);

    const Spline3 still({{2.0F, 2.0F, 2.0F}, {2.0F, 2.0F, 2.0F}});
    EXPECT_EQ(still.getLength(), 0.0F);
    EXPECT_THROW(static_cast<void>(still.getTangent(0.5F)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(still.getFrames(3)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Spline3(two).getFrames(1)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Spline3(two).getFrames(4, {})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Spline3(two).sampleByDistance(0.0F)), std::invalid_argument);
    EXPECT_EQ(Spline3::kindFromName("nurbs"), Spline3::Kind::Nurbs);
    EXPECT_EQ(Spline3::kindName(Spline3::Kind::BSpline), "bSpline");
}

} // namespace haylen::math
