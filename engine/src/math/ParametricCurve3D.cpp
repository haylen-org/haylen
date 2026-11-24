#include "haylen/math/ParametricCurve3D.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <utility>

#include "haylen/math/Math.hpp"
#include "math/ArcLength.hpp"
#include "math/Curve3DHelper.hpp"

namespace haylen::math {

ParametricCurve3D::ParametricCurve3D(Function curveFunction, const Options& options) : function(std::move(curveFunction)), closed(options.closed) {
    if (!function) {
        throw std::invalid_argument("A parametric curve needs a function that gives its points.");
    }

    if (options.steps < 1 || options.steps > kMaxSteps) {
        throw std::invalid_argument("A parametric curve measures its length in 1 to 1048576 steps.");
    }

    if (closed) {
        const Vec3 start = function(0.0F).point;
        const Vec3 end = function(1.0F).point;
        const float scale = std::max({1.0F, start.getLength(), end.getLength()});
        if (!(Vec3::distance(start, end) <= kCloseTolerance * scale)) {
            throw std::invalid_argument("A closed parametric curve must end where it starts.");
        }
    }

    Curve3DHelper::measure(*this, options.steps, lengths);
}

// The angle and the radius change at a steady rate along the parameter, so the derivatives follow from the product rule.
ParametricCurve3D ParametricCurve3D::helix(const Helix& helix) {
    const float endRadius = helix.endRadius.value_or(helix.radius);
    if (!(std::isfinite(helix.radius) && helix.radius >= 0.0F && std::isfinite(endRadius) && endRadius >= 0.0F)) {
        throw std::invalid_argument("The radius and the end radius of a helix must be finite numbers of 0 or more.");
    }

    if (!std::isfinite(helix.pitch)) {
        throw std::invalid_argument("The pitch of a helix must be a finite number.");
    }

    if (!std::isfinite(helix.turns) || helix.turns == 0.0F || std::fabs(helix.turns) > kMaxTurns) {
        throw std::invalid_argument("A helix needs a finite number of turns other than 0 and at most 16384 either way.");
    }

    const double spin = Math::kTau * static_cast<double>(helix.turns);
    const double widen = static_cast<double>(endRadius) - helix.radius;
    const double rise = static_cast<double>(helix.pitch) * std::fabs(helix.turns);
    const double radius = helix.radius;
    // clang-format off
    Function evaluate = [spin, widen, rise, radius](float t) {
        const double angle = spin * t;
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        const double reach = radius + widen * t;
        const Vec3 point{static_cast<float>(reach * cosine), static_cast<float>(rise * t), static_cast<float>(-reach * sine)};
        const Vec3 first{static_cast<float>(widen * cosine - reach * spin * sine), static_cast<float>(rise), static_cast<float>(-(widen * sine + reach * spin * cosine))};
        const Vec3 second{static_cast<float>(-2.0 * widen * spin * sine - reach * spin * spin * cosine), 0.0F, static_cast<float>(-(2.0 * widen * spin * cosine - reach * spin * spin * sine))};
        return Derivatives{.point = point, .first = first, .second = second};
    };
    // clang-format on
    const auto steps = std::max<std::size_t>(kDefaultSteps, static_cast<std::size_t>(std::ceil(std::fabs(helix.turns) * static_cast<float>(kStepsPerTurn))));
    return ParametricCurve3D(std::move(evaluate), {.steps = steps});
}

// The ring turns `p` times around the up axis while the point turns `q` times around the circle of the tube, so the knot closes after one walk.
ParametricCurve3D ParametricCurve3D::torusKnot(const TorusKnot& knot) {
    if (knot.p < 1 || knot.q < 1 || knot.p > kMaxWindings || knot.q > kMaxWindings || std::gcd(knot.p, knot.q) != 1) {
        throw std::invalid_argument("The windings p and q of a torus knot must be whole numbers from 1 to 1024 with no common divisor above 1, or the knot runs over itself.");
    }

    if (!(std::isfinite(knot.radius) && std::isfinite(knot.tubeRadius) && knot.tubeRadius > 0.0F && knot.tubeRadius < knot.radius)) {
        throw std::invalid_argument("A torus knot needs a finite tube radius above 0 and below its radius.");
    }

    const double around = Math::kTau * static_cast<double>(knot.p);
    const double through = Math::kTau * static_cast<double>(knot.q);
    const double ring = knot.radius;
    const double tube = knot.tubeRadius;
    // clang-format off
    Function evaluate = [around, through, ring, tube](float t) {
        const double u = around * t;
        const double v = through * t;
        const double reach = ring + tube * std::cos(v);
        const double reachFirst = -tube * through * std::sin(v);
        const double reachSecond = -tube * through * through * std::cos(v);
        const double cosine = std::cos(u);
        const double sine = std::sin(u);
        const Vec3 point{static_cast<float>(reach * cosine), static_cast<float>(tube * std::sin(v)), static_cast<float>(-reach * sine)};
        const Vec3 first{static_cast<float>(reachFirst * cosine - reach * around * sine), static_cast<float>(tube * through * std::cos(v)), static_cast<float>(-(reachFirst * sine + reach * around * cosine))};
        const double secondX = reachSecond * cosine - 2.0 * reachFirst * around * sine - reach * around * around * cosine;
        const double secondZ = -(reachSecond * sine + 2.0 * reachFirst * around * cosine - reach * around * around * sine);
        const Vec3 second{static_cast<float>(secondX), static_cast<float>(-tube * through * through * std::sin(v)), static_cast<float>(secondZ)};
        return Derivatives{.point = point, .first = first, .second = second};
    };
    // clang-format on
    const auto steps = std::max<std::size_t>(kDefaultSteps, static_cast<std::size_t>(knot.p + knot.q) * kStepsPerTurn);
    return ParametricCurve3D(std::move(evaluate), {.closed = true, .steps = steps});
}

ParametricCurve3D ParametricCurve3D::lissajous(const Lissajous& figure) {
    if (!figure.amplitude.isFinite() || !figure.phase.isFinite()) {
        throw std::invalid_argument("The amplitude and the phase of a Lissajous curve must be finite.");
    }

    const std::array<float, 3> frequencies{figure.frequency.x, figure.frequency.y, figure.frequency.z};
    for (const float frequency : frequencies) {
        if (!(frequency >= 1.0F && frequency <= static_cast<float>(kMaxWindings) && std::floor(frequency) == frequency)) {
            throw std::invalid_argument("The frequencies of a Lissajous curve must be whole numbers from 1 to 1024.");
        }
    }

    const Vec3 amplitude = figure.amplitude;
    const Vec3 rate = figure.frequency * Math::kTau;
    const Vec3 phase = figure.phase;
    // clang-format off
    Function evaluate = [amplitude, rate, phase](float t) {
        const Vec3 angle = rate * t + phase;
        const Vec3 sine{std::sin(angle.x), std::sin(angle.y), std::sin(angle.z)};
        const Vec3 cosine{std::cos(angle.x), std::cos(angle.y), std::cos(angle.z)};
        return Derivatives{.point = amplitude * sine, .first = amplitude * rate * cosine, .second = -(amplitude * rate * rate * sine)};
    };
    // clang-format on
    const auto turns = static_cast<std::size_t>(figure.frequency.x + figure.frequency.y + figure.frequency.z);
    return ParametricCurve3D(std::move(evaluate), {.closed = true, .steps = std::max<std::size_t>(kDefaultSteps, turns * kStepsPerTurn)});
}

float ParametricCurve3D::wrap(float t) const noexcept {
    return closed ? t - std::floor(t) : std::clamp(t, 0.0F, 1.0F);
}

Vec3 ParametricCurve3D::getPoint(float t) const {
    return function(wrap(t)).point;
}

ParametricCurve3D::Derivatives ParametricCurve3D::getDerivatives(float t) const {
    return function(wrap(t));
}

Vec3 ParametricCurve3D::getTangent(float t) const {
    const std::optional<Vec3> direction = Curve3DHelper::getDirection(getDerivatives(t));
    if (!direction) {
        throw std::invalid_argument("The curve stands still at this parameter, so it has no direction.");
    }

    return *direction;
}

float ParametricCurve3D::getParameterAtDistance(float distance) const noexcept {
    return ArcLength::getParameter(lengths, distance, closed);
}

Vec3 ParametricCurve3D::getPointAtDistance(float distance) const {
    return getPoint(getParameterAtDistance(distance));
}

Vec3 ParametricCurve3D::getTangentAtDistance(float distance) const {
    return getTangent(getParameterAtDistance(distance));
}

ParametricCurve3D::ClosestPoint ParametricCurve3D::getClosestPoint(Vec3 point) const {
    return Curve3DHelper::findClosest(*this, point, lengths.size() - 1);
}

void ParametricCurve3D::sample(std::span<Vec3> points) const {
    Curve3DHelper::sample(*this, points);
}

std::vector<Vec3> ParametricCurve3D::sample(std::size_t count) const {
    return Curve3DHelper::sample(*this, count);
}

std::vector<Vec3> ParametricCurve3D::sampleEvenly(std::size_t count) const {
    return Curve3DHelper::sampleEvenly(*this, count);
}

std::vector<Vec3> ParametricCurve3D::sampleByDistance(float spacing) const {
    return Curve3DHelper::sampleByDistance(*this, spacing);
}

std::vector<CurveFrame> ParametricCurve3D::getFrames(std::size_t count, Vec3 up) const {
    return Curve3DHelper::getFrames(*this, count, up);
}

} // namespace haylen::math
