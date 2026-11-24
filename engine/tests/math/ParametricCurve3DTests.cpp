#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "haylen/math/CurveFrame.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/ParametricCurve3D.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

class ParametricCurve3DTest : public ::testing::Test {
  protected:
    static constexpr float kStep = 1e-3F;

    static void expectNear(Vec3 actual, Vec3 expected, float tolerance) {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }

    // Compares the derivatives of the curve with the change of its points over a small step at many parameters.
    static void expectDerivativesMatchTheChange(const ParametricCurve3D& curve, float tolerance) {
        for (int step = 1; step < 40; ++step) {
            const float t = static_cast<float>(step) / 40.0F;
            const ParametricCurve3D::Derivatives derivatives = curve.getDerivatives(t);
            const ParametricCurve3D::Derivatives before = curve.getDerivatives(t - kStep);
            const ParametricCurve3D::Derivatives after = curve.getDerivatives(t + kStep);
            const float speed = derivatives.first.getLength();
            const float bend = derivatives.second.getLength();
            expectNear((after.point - before.point) / (2.0F * kStep), derivatives.first, tolerance * speed);
            expectNear((after.first - before.first) / (2.0F * kStep), derivatives.second, tolerance * bend);
        }
    }
};

// A helix keeps its radius around the up axis and rises its pitch with every turn, and a spiral widens from its radius to its end radius.
TEST_F(ParametricCurve3DTest, WindsHelicesAndSpirals) {
    const ParametricCurve3D spring = ParametricCurve3D::helix({.radius = 2.0F, .pitch = 0.5F, .turns = 3.0F});
    EXPECT_FALSE(spring.isClosed());
    for (int step = 0; step <= 30; ++step) {
        const Vec3 point = spring.getPoint(static_cast<float>(step) / 30.0F);
        EXPECT_NEAR(std::hypot(point.x, point.z), 2.0F, 1e-5F) << step;
        EXPECT_NEAR(point.y, 1.5F * static_cast<float>(step) / 30.0F, 1e-5F) << step;
    }

    // Each turn runs along the hypotenuse of its circumference and its pitch.
    EXPECT_NEAR(spring.getLength(), 3.0F * std::hypot(2.0F * Math::kTau, 0.5F), 2e-2F);
    expectNear(spring.getPoint(1.0F / 12.0F), {0.0F, 0.125F, -2.0F}, 1e-5F);
    expectDerivativesMatchTheChange(spring, 1e-3F);

    const ParametricCurve3D spiral = ParametricCurve3D::helix({.radius = 1.0F, .endRadius = 3.0F, .pitch = 0.0F, .turns = -2.0F});
    expectNear(spiral.getPoint(1.0F), {3.0F, 0.0F, 0.0F}, 1e-4F);
    expectNear(spiral.getPoint(0.125F), {0.0F, 0.0F, 1.25F}, 1e-4F);
    expectDerivativesMatchTheChange(spiral, 1e-3F);
}

// A torus knot stays on its torus and closes after one walk, so its frames meet and its distances wrap around.
TEST_F(ParametricCurve3DTest, TiesTorusKnotsThatClose) {
    const ParametricCurve3D trefoil = ParametricCurve3D::torusKnot({});
    EXPECT_TRUE(trefoil.isClosed());
    for (int step = 0; step <= 60; ++step) {
        const Vec3 point = trefoil.getPoint(static_cast<float>(step) / 60.0F);
        const float ring = std::hypot(point.x, point.z) - 1.0F;
        EXPECT_NEAR(std::hypot(ring, point.y), 0.4F, 1e-5F) << step;
    }

    expectNear(trefoil.getPoint(1.0F), trefoil.getPoint(0.0F), 1e-5F);
    expectNear(trefoil.getPointAtDistance(trefoil.getLength() + 0.3F), trefoil.getPointAtDistance(0.3F), 1e-4F);
    expectDerivativesMatchTheChange(trefoil, 2e-3F);

    const std::vector<CurveFrame> frames = trefoil.getFrames(32);
    expectNear(frames.back().position, frames.front().position, 1e-4F);
    expectNear(frames.back().normal, frames.front().normal, 1e-3F);
    for (const CurveFrame& frame : frames) {
        EXPECT_NEAR(Vec3::dot(frame.tangent, frame.normal), 0.0F, 1e-4F);
    }
}

TEST_F(ParametricCurve3DTest, SwingsLissajousFiguresAndSamplesThemInBatches) {
    const ParametricCurve3D figure = ParametricCurve3D::lissajous({.amplitude = {2.0F, 1.0F, 3.0F}, .frequency = {1.0F, 2.0F, 3.0F}, .phase = {Math::kHalfPi, 0.0F, 0.0F}});
    EXPECT_TRUE(figure.isClosed());
    expectNear(figure.getPoint(0.0F), {2.0F, 0.0F, 0.0F}, 1e-5F);
    expectNear(figure.getPoint(0.25F), {0.0F, 0.0F, -3.0F}, 1e-5F);
    expectDerivativesMatchTheChange(figure, 2e-3F);

    // The batch fills the points spread evenly over the parameter, the same as one point at a time.
    std::vector<Vec3> points(9);
    figure.sample(points);
    for (std::size_t index = 0; index < points.size(); ++index) {
        expectNear(points[index], figure.getPoint(static_cast<float>(index) / 8.0F), 0.0F);
    }

    EXPECT_EQ(figure.sample(5).size(), 5U);
    EXPECT_EQ(figure.sampleEvenly(7).size(), 7U);
    const std::vector<Vec3> spaced = figure.sampleByDistance(figure.getLength() / 4.5F);
    EXPECT_EQ(spaced.size(), 5U);
}

// A curve that a function of the code defines walks by distance and finds its closest point like the curves of the engine.
TEST_F(ParametricCurve3DTest, WalksCurvesOfAnyFunction) {
    // clang-format off
    const ParametricCurve3D wave([](float t) {
        const float angle = Math::kTau * t;
        return ParametricCurve3D::Derivatives{.point = {10.0F * t, std::sin(angle), 0.0F}, .first = {10.0F, Math::kTau * std::cos(angle), 0.0F}, .second = {0.0F, -Math::kTau * Math::kTau * std::sin(angle), 0.0F}};
    }, {.steps = 512});
    // clang-format on
    EXPECT_FALSE(wave.isClosed());
    const ParametricCurve3D::ClosestPoint closest = wave.getClosestPoint({2.5F, 3.0F, 0.0F});
    EXPECT_NEAR(closest.t, 0.25F, 1e-3F);
    EXPECT_NEAR(closest.distance, 2.0F, 1e-3F);
    EXPECT_NEAR(wave.getParameterAtDistance(wave.getLength()), 1.0F, 1e-6F);
    expectNear(wave.getTangent(0.0F), Vec3{10.0F, Math::kTau, 0.0F}.getNormalized(), 1e-5F);

    const ParametricCurve3D still([](float) { return ParametricCurve3D::Derivatives{.point = {1.0F, 1.0F, 1.0F}}; }, {});
    EXPECT_EQ(still.getLength(), 0.0F);
    EXPECT_THROW(static_cast<void>(still.getTangent(0.5F)), std::invalid_argument);
}

TEST_F(ParametricCurve3DTest, RefusesCurvesThatDoNotExist) {
    EXPECT_THROW(ParametricCurve3D({}, {}), std::invalid_argument);
    EXPECT_THROW(ParametricCurve3D([](float t) { return ParametricCurve3D::Derivatives{.point = {t, 0.0F, 0.0F}}; }, {.steps = 0}), std::invalid_argument);
    EXPECT_THROW(ParametricCurve3D([](float t) { return ParametricCurve3D::Derivatives{.point = {t, 0.0F, 0.0F}}; }, {.closed = true}), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(ParametricCurve3D::helix({.turns = 0.0F})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(ParametricCurve3D::helix({.turns = 20000.0F})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(ParametricCurve3D::helix({.radius = -1.0F})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(ParametricCurve3D::helix({.pitch = std::numeric_limits<float>::infinity()})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(ParametricCurve3D::torusKnot({.p = 2, .q = 4})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(ParametricCurve3D::torusKnot({.p = 0})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(ParametricCurve3D::torusKnot({.radius = 1.0F, .tubeRadius = 1.0F})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(ParametricCurve3D::lissajous({.frequency = {1.5F, 2.0F, 3.0F}})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(ParametricCurve3D::lissajous({.frequency = {0.0F, 2.0F, 3.0F}})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(ParametricCurve3D::lissajous({.amplitude = {std::numeric_limits<float>::quiet_NaN(), 1.0F, 1.0F}})), std::invalid_argument);
}

} // namespace haylen::math
