#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/CurveFrame.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A smooth curve in 3D built from control points and measured by arc length, such as a rail, a tube, a road or the path of a camera. Parameters run from 0 at the start to 1 at the end of the whole curve. Every kind is held as a NURBS curve, so points, derivatives and frames come from one exact evaluation.
class Spline3D final {
  public:
    // Linear curves join their points with straight segments. Catmull-Rom curves pass through every point, with the parameterization that `alpha` picks and the `tension` that shortens their tangents. Quadratic Bezier curves chain segments whose points go end, control, end and so on, so open curves take 2n + 1 points and closed ones 2n, and cubic Bezier curves go end, control, control, end, so open curves take 3n + 1 points and closed ones 3n. B-splines stay inside the hull of the points without passing through them, except for the ends of open curves. NURBS curves take a weight for each point, a degree and knots.
    enum class Kind : std::uint8_t {
        Linear,
        CatmullRom,
        QuadraticBezier,
        Bezier,
        BSpline,
        Nurbs,
    };

    // The `alpha` of a Catmull-Rom curve is 0 for the uniform parameterization, 0.5 for the centripetal one, which never loops or cusps inside a segment, and 1 for the chordal one, and its `tension` from 0 to 1 shortens its tangents until the curve runs straight at 1. The weights, knots and degree belong to NURBS curves alone. Empty weights give every point a weight of 1, and empty knots make the clamped uniform knots, which reach both end points.
    struct Options {
        Kind kind = Kind::CatmullRom;
        bool closed = false;
        float alpha = 0.5F;
        float tension = 0.0F;
        std::vector<float> weights;
        std::vector<float> knots;
        int degree = 3;
    };

    // The point and its first and second derivatives by the parameter, which give the velocity and the acceleration of a walk along the whole curve from 0 to 1.
    struct Derivatives {
        Vec3 point;
        Vec3 first;
        Vec3 second;
    };

    struct ClosestPoint {
        float t = 0.0F;
        Vec3 point;
        float distance = 0.0F;
    };

    // Throws `std::invalid_argument` when the kind needs more points or a different count, for an alpha or a tension outside 0 to 1, or for weights, knots or a degree that do not describe a NURBS curve.
    explicit Spline3D(std::vector<Vec3> controlPoints, const Options& options);
    explicit Spline3D(std::vector<Vec3> controlPoints);

    [[nodiscard]] static std::optional<Kind> kindFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view kindName(Kind value) noexcept;

    [[nodiscard]] const std::vector<Vec3>& getPoints() const noexcept {
        return points;
    }
    [[nodiscard]] Kind getKind() const noexcept {
        return kind;
    }
    [[nodiscard]] bool isClosed() const noexcept {
        return closed;
    }
    [[nodiscard]] int getDegree() const noexcept {
        return degree;
    }
    // The weights and knots of the NURBS curve that holds the spline, which for the other kinds come from their points.
    [[nodiscard]] const std::vector<float>& getWeights() const noexcept {
        return weights;
    }
    [[nodiscard]] const std::vector<float>& getKnots() const noexcept {
        return knots;
    }
    // The pieces between knots that differ, each a polynomial of the degree or a ratio of two of them.
    [[nodiscard]] std::size_t getSegmentCount() const noexcept {
        return segmentCount;
    }
    [[nodiscard]] float getLength() const noexcept {
        return lengths.back();
    }

    [[nodiscard]] Vec3 getPoint(float t) const noexcept;
    [[nodiscard]] Derivatives getDerivatives(float t) const noexcept;
    // Returns the unit direction of travel, which at a point where the curve stops for an instant follows the way it leaves. Throws `std::invalid_argument` where the curve stands still, so it has no direction.
    [[nodiscard]] Vec3 getTangent(float t) const;
    [[nodiscard]] Vec3 getPointAtDistance(float distance) const noexcept;
    [[nodiscard]] Vec3 getTangentAtDistance(float distance) const;
    [[nodiscard]] float getParameterAtDistance(float distance) const noexcept;
    [[nodiscard]] ClosestPoint getClosestPoint(Vec3 point) const noexcept;

    // Returns `count` points spread evenly over the parameter, both ends included. Throws `std::invalid_argument` for more than 16777216 points.
    [[nodiscard]] std::vector<Vec3> sample(std::size_t count) const;
    // Returns `count` points spread evenly by distance along the curve, both ends included. Throws `std::invalid_argument` for more than 16777216 points.
    [[nodiscard]] std::vector<Vec3> sampleEvenly(std::size_t count) const;
    // Returns points `spacing` apart along the curve from its start, plus the end point of open curves. Throws `std::invalid_argument` for a spacing that is not finite and above 0 or that would take more than 16777216 steps along the curve.
    [[nodiscard]] std::vector<Vec3> sampleByDistance(float spacing) const;

    // Returns `count` frames spread evenly by distance, both ends included, whose normals parallel transport carries along the curve, as `ParallelTransport` describes. Throws `std::invalid_argument` for fewer than 2 or more than 16777216 frames, for an `up` without a direction or where the curve has no direction.
    [[nodiscard]] std::vector<CurveFrame> getFrames(std::size_t count, Vec3 up = Vec3::up()) const;

  private:
    static const std::array<std::pair<std::string_view, Kind>, 6> kKindNames;
    static constexpr std::size_t kStepsPerSegment = 32;
    static constexpr int kClosestIterations = 8;

    void buildLinear();
    void buildCatmullRom(float alpha, float tension);
    void buildBezier(int segmentDegree);
    void buildBSpline();
    void buildNurbs(const Options& options);
    void appendBezierSegment(std::span<const Vec3> segment);
    void measure();

    [[nodiscard]] Vec3 wrappedPoint(std::ptrdiff_t index) const noexcept;
    [[nodiscard]] float toKnot(float t) const noexcept;
    [[nodiscard]] Derivatives evaluate(float u) const noexcept;
    [[nodiscard]] float refineClosest(Vec3 point, float t) const noexcept;

    std::vector<Vec3> points;
    Kind kind;
    bool closed;
    int degree = 3;
    std::vector<Vec3> controls;
    std::vector<float> weights;
    std::vector<float> knots;
    std::size_t segmentCount = 0;
    std::vector<float> lengths;
};

} // namespace haylen::math
