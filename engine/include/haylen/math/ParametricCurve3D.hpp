#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "haylen/math/CurveFrame.hpp"
#include "haylen/math/Spline3D.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A curve in 3D that a function of its parameter defines, such as a helix, a spring, a torus knot or a Lissajous figure, measured by arc length like a spline. Parameters run from 0 at the start to 1 at the end, and the batch methods evaluate many points in one call.
class ParametricCurve3D final {
  public:
    using Derivatives = Spline3D::Derivatives;
    using ClosestPoint = Spline3D::ClosestPoint;

    // Answers the point and its first and second derivatives by the parameter, from 0 to 1.
    using Function = std::function<Derivatives(float)>;

    // The steps that measure the length of a curve unless it asks for more, and the most it takes.
    static constexpr std::size_t kDefaultSteps = 256;
    static constexpr std::size_t kMaxSteps = std::size_t{1} << 20;

    // A closed curve ends where it starts, so its parameters and distances wrap around. The steps are the evenly spaced parameters at which the curve records its length and starts the search for its closest point, enough to follow every turn of the curve.
    struct Options {
        bool closed = false;
        std::size_t steps = kDefaultSteps;
    };

    // A helix winds around the up axis by the right hand from the point at `radius` along the right axis, rising `pitch` with each turn. An `endRadius` that differs from the radius widens or narrows it into a spiral, and negative turns wind it the other way.
    struct Helix {
        float radius = 1.0F;
        std::optional<float> endRadius;
        float pitch = 1.0F;
        float turns = 1.0F;
    };

    // A torus knot winds `p` times around the up axis and `q` times through the tube of a torus whose ring has the `radius` and whose tube has the `tubeRadius`, such as the trefoil of `p` 2 and `q` 3.
    struct TorusKnot {
        int p = 2;
        int q = 3;
        float radius = 1.0F;
        float tubeRadius = 0.4F;
    };

    // A Lissajous curve swings along each axis as `amplitude * sin(tau * frequency * t + phase)`, with whole frequencies, so it closes after one walk from 0 to 1.
    struct Lissajous {
        Vec3 amplitude{1.0F, 1.0F, 1.0F};
        Vec3 frequency{1.0F, 2.0F, 3.0F};
        Vec3 phase;
    };

    // Throws `std::invalid_argument` for an empty function, for steps outside 1 to `kMaxSteps`, or for a closed curve that does not end where it starts.
    ParametricCurve3D(Function function, const Options& options);

    // Each throws `std::invalid_argument` for values that do not describe its curve, with a sentence that names the value.
    [[nodiscard]] static ParametricCurve3D helix(const Helix& helix);
    [[nodiscard]] static ParametricCurve3D torusKnot(const TorusKnot& knot);
    [[nodiscard]] static ParametricCurve3D lissajous(const Lissajous& figure);

    [[nodiscard]] bool isClosed() const noexcept {
        return closed;
    }
    [[nodiscard]] float getLength() const noexcept {
        return lengths.back();
    }

    [[nodiscard]] Vec3 getPoint(float t) const;
    [[nodiscard]] Derivatives getDerivatives(float t) const;
    // Returns the unit direction of travel, which at a point where the curve stops for an instant follows the way it leaves. Throws `std::invalid_argument` where the curve stands still, so it has no direction.
    [[nodiscard]] Vec3 getTangent(float t) const;
    [[nodiscard]] Vec3 getPointAtDistance(float distance) const;
    [[nodiscard]] Vec3 getTangentAtDistance(float distance) const;
    [[nodiscard]] float getParameterAtDistance(float distance) const noexcept;
    [[nodiscard]] ClosestPoint getClosestPoint(Vec3 point) const;

    // Fills the points spread evenly over the parameter, both ends included, in one pass.
    void sample(std::span<Vec3> points) const;
    // Returns `count` points spread evenly over the parameter, both ends included. Throws `std::invalid_argument` for more than 16777216 points.
    [[nodiscard]] std::vector<Vec3> sample(std::size_t count) const;
    // Returns `count` points spread evenly by distance along the curve, both ends included. Throws `std::invalid_argument` for more than 16777216 points.
    [[nodiscard]] std::vector<Vec3> sampleEvenly(std::size_t count) const;
    // Returns points `spacing` apart along the curve from its start, plus the end point of an open curve, with the errors of `Spline3D::sampleByDistance`.
    [[nodiscard]] std::vector<Vec3> sampleByDistance(float spacing) const;
    // Returns `count` frames spread evenly by distance, as `Spline3D::getFrames` describes.
    [[nodiscard]] std::vector<CurveFrame> getFrames(std::size_t count, Vec3 up = Vec3::up()) const;

  private:
    static constexpr std::size_t kStepsPerTurn = 64;
    static constexpr float kMaxTurns = 16384.0F;
    static constexpr int kMaxWindings = 1024;
    static constexpr float kCloseTolerance = 1e-4F;

    [[nodiscard]] float wrap(float t) const noexcept;

    Function function;
    bool closed;
    std::vector<float> lengths;
};

} // namespace haylen::math
