#include "haylen/math/Spline3.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "math/ArcLength.hpp"
#include "math/NurbsBasis.hpp"
#include "math/ParallelTransport.hpp"

namespace haylen::math {

const std::array<std::pair<std::string_view, Spline3::Kind>, 6> Spline3::kKindNames{{{"linear", Kind::Linear}, {"catmullRom", Kind::CatmullRom}, {"quadraticBezier", Kind::QuadraticBezier}, {"bezier", Kind::Bezier}, {"bSpline", Kind::BSpline}, {"nurbs", Kind::Nurbs}}};

Spline3::Spline3(std::vector<Vec3> controlPoints) : Spline3(std::move(controlPoints), Options{}) {}

Spline3::Spline3(std::vector<Vec3> controlPoints, const Options& options) : points(std::move(controlPoints)), kind(options.kind), closed(options.closed) {
    if (kind != Kind::Nurbs && (!options.weights.empty() || !options.knots.empty() || options.degree != 3)) {
        throw std::invalid_argument("Only a NURBS spline takes weights, knots and a degree.");
    }

    if (kind != Kind::CatmullRom && (options.alpha != 0.5F || options.tension != 0.0F)) {
        throw std::invalid_argument("Only a Catmull-Rom spline takes an alpha and a tension.");
    }

    switch (kind) {
    case Kind::Linear:
        buildLinear();
        break;
    case Kind::CatmullRom:
        buildCatmullRom(options.alpha, options.tension);
        break;
    case Kind::QuadraticBezier:
        buildBezier(2);
        break;
    case Kind::Bezier:
        buildBezier(3);
        break;
    case Kind::BSpline:
        buildBSpline();
        break;
    case Kind::Nurbs:
        buildNurbs(options);
        break;
    }

    if (kind != Kind::Nurbs) {
        weights.assign(controls.size(), 1.0F);
    }

    for (std::size_t index = static_cast<std::size_t>(degree); index < controls.size(); ++index) {
        segmentCount += knots[index] < knots[index + 1] ? 1U : 0U;
    }

    measure();
}

std::optional<Spline3::Kind> Spline3::kindFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kKindNames, name, &std::pair<std::string_view, Kind>::first);
    return found != kKindNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Spline3::kindName(Kind value) noexcept {
    return std::ranges::find(kKindNames, value, &std::pair<std::string_view, Kind>::second)->first;
}

// Closed curves wrap indices around and open curves clamp them to the end points.
Vec3 Spline3::wrappedPoint(std::ptrdiff_t index) const noexcept {
    const auto count = static_cast<std::ptrdiff_t>(points.size());
    if (closed) {
        return points[static_cast<std::size_t>(((index % count) + count) % count)];
    }

    return points[static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(index, 0, count - 1))];
}

void Spline3::buildLinear() {
    if (points.size() < (closed ? 3U : 2U)) {
        throw std::invalid_argument("A linear spline needs at least two points when open and three when closed.");
    }

    degree = 1;
    controls = points;
    if (closed) {
        controls.push_back(points.front());
    }

    knots = NurbsBasis::makeClampedKnots(controls.size(), degree);
}

// Each segment is the cubic of the pyramid of Barry and Goldman, which its ends and the derivatives there fix, so it becomes a Bezier segment exactly. The tension shortens both derivatives, and open curves extend their ends by mirroring the neighboring point.
void Spline3::buildCatmullRom(float alpha, float tension) {
    if (!(alpha >= 0.0F && alpha <= 1.0F) || !(tension >= 0.0F && tension <= 1.0F)) {
        throw std::invalid_argument("A Catmull-Rom spline needs an alpha and a tension from 0 to 1.");
    }

    if (points.size() < (closed ? 3U : 2U)) {
        throw std::invalid_argument("A Catmull-Rom spline needs at least two points when open and three when closed.");
    }

    const std::size_t segments = closed ? points.size() : points.size() - 1;
    for (std::size_t segment = 0; segment < segments; ++segment) {
        const auto index = static_cast<std::ptrdiff_t>(segment);
        const Vec3 p1 = wrappedPoint(index);
        const Vec3 p2 = wrappedPoint(index + 1);
        const Vec3 p0 = !closed && segment == 0 ? p1 * 2.0F - p2 : wrappedPoint(index - 1);
        const Vec3 p3 = !closed && segment + 1 == segments ? p2 * 2.0F - p1 : wrappedPoint(index + 2);

        const auto knot = [alpha](Vec3 from, Vec3 to) { return std::max(std::pow(Vec3::distance(from, to), alpha), 1e-4F); };
        const float t1 = knot(p0, p1);
        const float t2 = t1 + knot(p1, p2);
        const float t3 = t2 + knot(p2, p3);
        const float span = t2 - t1;
        const float reach = span * (1.0F - tension) / 3.0F;
        const Vec3 startVelocity = (p1 - p0) / t1 - (p2 - p0) / t2 + (p2 - p1) / span;
        const Vec3 endVelocity = (p2 - p1) / span - (p3 - p1) / (t3 - t1) + (p3 - p2) / (t3 - t2);
        const std::array<Vec3, 4> bezier{p1, p1 + startVelocity * reach, p2 - endVelocity * reach, p2};
        appendBezierSegment(bezier);
    }

    knots.push_back(knots.back());
}

void Spline3::buildBezier(int segmentDegree) {
    const auto step = static_cast<std::size_t>(segmentDegree);
    const std::size_t count = points.size();
    const bool valid = closed ? count >= step && count % step == 0 : count >= step + 1 && (count - 1) % step == 0;
    if (!valid && segmentDegree == 2) {
        throw std::invalid_argument("A quadratic Bezier spline needs 2n + 1 points when open and 2n points when closed.");
    }

    if (!valid) {
        throw std::invalid_argument("A Bezier spline needs 3n + 1 points when open and 3n points when closed.");
    }

    degree = segmentDegree;
    std::array<Vec3, 4> segment{};
    for (std::size_t first = 0; first + step <= (closed ? count : count - 1); first += step) {
        for (std::size_t offset = 0; offset <= step; ++offset) {
            segment[offset] = wrappedPoint(static_cast<std::ptrdiff_t>(first + offset));
        }

        appendBezierSegment(std::span<const Vec3>(segment).first(step + 1));
    }

    knots.push_back(knots.back());
}

// The uniform cubic B-spline takes its points with uniform knots. Open curves repeat their end points three times so the curve reaches them, and closed curves repeat their first three points after the last.
void Spline3::buildBSpline() {
    if (points.size() < (closed ? 3U : 2U)) {
        throw std::invalid_argument("A B-spline needs at least two points when open and three when closed.");
    }

    const std::size_t count = closed ? points.size() + 3 : points.size() + 4;
    for (std::size_t index = 0; index < count; ++index) {
        controls.push_back(wrappedPoint(static_cast<std::ptrdiff_t>(index) - 2));
    }

    for (std::size_t index = 0; index < count + 4; ++index) {
        knots.push_back(static_cast<float>(index));
    }
}

void Spline3::buildNurbs(const Options& options) {
    if (closed) {
        throw std::invalid_argument("A NURBS spline closes through its own points and knots, so it takes no option closed.");
    }

    const std::size_t count = points.size();
    weights = NurbsBasis::checkWeights("NURBS spline", options.weights, count);
    degree = options.degree;
    const bool fitsDegree = degree >= 1 && degree <= NurbsBasis::kMaxDegree && count > static_cast<std::size_t>(degree);
    knots = options.knots.empty() && fitsDegree ? NurbsBasis::makeClampedKnots(count, degree) : options.knots;
    NurbsBasis::check("NURBS spline", count, degree, knots);
    controls = points;
}

// The segments share their end points, so each one adds the points after its first and the knot of its end once for every degree.
void Spline3::appendBezierSegment(std::span<const Vec3> segment) {
    if (controls.empty()) {
        controls.push_back(segment.front());
        knots.assign(segment.size(), 0.0F);
    }

    controls.insert(controls.end(), segment.begin() + 1, segment.end());
    const float knot = knots.back() + 1.0F;
    knots.insert(knots.end(), segment.size() - 1, knot);
}

// Records the arc length at evenly spaced parameters, which distance lookups interpolate.
void Spline3::measure() {
    const std::size_t steps = segmentCount * kStepsPerSegment;
    lengths.assign(steps + 1, 0.0F);
    Vec3 previous = getPoint(0.0F);
    for (std::size_t step = 1; step <= steps; ++step) {
        const Vec3 point = evaluate(toKnot(static_cast<float>(step) / static_cast<float>(steps))).point;
        lengths[step] = lengths[step - 1] + Vec3::distance(previous, point);
        previous = point;
    }
}

float Spline3::toKnot(float t) const noexcept {
    const float low = knots[static_cast<std::size_t>(degree)];
    const float high = knots[controls.size()];
    const float wrapped = closed ? t - std::floor(t) : std::clamp(t, 0.0F, 1.0F);
    return low + (high - low) * wrapped;
}

// The point is the ratio of the weighted points to the weights, and its derivatives follow by the rule of quotients. A parameter that is not a number gives a point that is not a number either.
Spline3::Derivatives Spline3::evaluate(float u) const noexcept {
    const auto p = static_cast<std::size_t>(degree);
    const std::size_t span = NurbsBasis::findSpan(knots, degree, controls.size(), u);
    NurbsBasis::Values basis{};
    NurbsBasis::evaluate(knots, degree, span, u, 2, basis);

    std::array<std::array<double, 3>, 3> weighted{};
    std::array<double, 3> weight{};
    for (std::size_t k = 0; k < 3; ++k) {
        for (std::size_t j = 0; j <= p; ++j) {
            const std::size_t index = span - p + j;
            const double scaled = basis[k][j] * weights[index];
            weighted[k][0] += scaled * controls[index].x;
            weighted[k][1] += scaled * controls[index].y;
            weighted[k][2] += scaled * controls[index].z;
            weight[k] += scaled;
        }
    }

    std::array<std::array<double, 3>, 3> curve{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        curve[0][axis] = weighted[0][axis] / weight[0];
        curve[1][axis] = (weighted[1][axis] - weight[1] * curve[0][axis]) / weight[0];
        curve[2][axis] = (weighted[2][axis] - 2.0 * weight[1] * curve[1][axis] - weight[2] * curve[0][axis]) / weight[0];
    }

    // The derivatives by the knot become derivatives by the parameter of the whole curve, which runs from 0 to 1.
    const double stretch = static_cast<double>(knots[controls.size()]) - knots[p];
    const auto toVector = [](const std::array<double, 3>& value, double scale) { return Vec3{static_cast<float>(value[0] * scale), static_cast<float>(value[1] * scale), static_cast<float>(value[2] * scale)}; };
    return {.point = toVector(curve[0], 1.0), .first = toVector(curve[1], stretch), .second = toVector(curve[2], stretch * stretch)};
}

Vec3 Spline3::getPoint(float t) const noexcept {
    return evaluate(toKnot(t)).point;
}

Spline3::Derivatives Spline3::getDerivatives(float t) const noexcept {
    return evaluate(toKnot(t));
}

Vec3 Spline3::getTangent(float t) const {
    const Derivatives derivatives = getDerivatives(t);
    if (!derivatives.first.isZero()) {
        return derivatives.first.getNormalized();
    }

    if (!derivatives.second.isZero()) {
        return derivatives.second.getNormalized();
    }

    throw std::invalid_argument("The spline stands still at this parameter, so it has no direction.");
}

float Spline3::getParameterAtDistance(float distance) const noexcept {
    return ArcLength::getParameter(lengths, distance, closed);
}

Vec3 Spline3::getPointAtDistance(float distance) const noexcept {
    return getPoint(getParameterAtDistance(distance));
}

Vec3 Spline3::getTangentAtDistance(float distance) const {
    return getTangent(getParameterAtDistance(distance));
}

// The nearest of the points of the table of lengths starts a search by Newton's method for the parameter where the line to the point stands at a right angle to the curve, kept between the neighbors of that point.
Spline3::ClosestPoint Spline3::getClosestPoint(Vec3 point) const noexcept {
    const std::size_t steps = lengths.size() - 1;
    std::size_t nearest = 0;
    float nearestSquared = std::numeric_limits<float>::infinity();
    for (std::size_t step = 0; step <= steps; ++step) {
        const float squared = Vec3::distanceSquared(evaluate(toKnot(static_cast<float>(step) / static_cast<float>(steps))).point, point);
        if (squared < nearestSquared) {
            nearestSquared = squared;
            nearest = step;
        }
    }

    const float start = static_cast<float>(nearest) / static_cast<float>(steps);
    const float refined = refineClosest(point, start);
    const float t = closed ? refined - std::floor(refined) : refined;
    const Vec3 found = getPoint(t);
    const float distance = Vec3::distance(found, point);
    if (distance * distance > nearestSquared) {
        return {.t = start, .point = getPoint(start), .distance = std::sqrt(nearestSquared)};
    }

    return {.t = t, .point = found, .distance = distance};
}

float Spline3::refineClosest(Vec3 point, float t) const noexcept {
    const float step = 1.0F / static_cast<float>(lengths.size() - 1);
    const float low = closed ? t - step : std::max(0.0F, t - step);
    const float high = closed ? t + step : std::min(1.0F, t + step);
    float current = t;
    for (int iteration = 0; iteration < kClosestIterations; ++iteration) {
        const Derivatives derivatives = getDerivatives(current);
        const Vec3 offset = derivatives.point - point;
        const float slope = Vec3::dot(offset, derivatives.first);
        const float curvature = Vec3::dot(derivatives.first, derivatives.first) + Vec3::dot(offset, derivatives.second);
        if (!(curvature > 0.0F)) {
            break;
        }

        const float next = std::clamp(current - slope / curvature, low, high);
        const bool settled = std::fabs(next - current) <= 1e-7F;
        current = next;
        if (settled) {
            break;
        }
    }

    return current;
}

std::vector<Vec3> Spline3::sample(std::size_t count) const {
    ArcLength::checkCount(count);
    std::vector<Vec3> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(getPoint(count == 1 ? 0.0F : static_cast<float>(index) / static_cast<float>(count - 1)));
    }

    return result;
}

std::vector<Vec3> Spline3::sampleEvenly(std::size_t count) const {
    ArcLength::checkCount(count);
    std::vector<Vec3> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const float share = count == 1 ? 0.0F : static_cast<float>(index) / static_cast<float>(count - 1);
        result.push_back(share >= 1.0F ? getPoint(1.0F) : getPointAtDistance(getLength() * share));
    }

    return result;
}

std::vector<Vec3> Spline3::sampleByDistance(float spacing) const {
    const std::size_t count = ArcLength::countSteps(getLength(), spacing);
    std::vector<Vec3> result;
    result.reserve(count + 1);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(getPointAtDistance(static_cast<float>(index) * spacing));
    }

    if (!closed) {
        result.push_back(getPoint(1.0F));
    }

    return result;
}

std::vector<CurveFrame> Spline3::getFrames(std::size_t count, Vec3 up) const {
    if (count < 2) {
        throw std::invalid_argument("The frames of a curve need at least 2 frames, one at each end.");
    }

    ArcLength::checkCount(count);
    std::vector<CurveFrame> frames(count);
    for (std::size_t index = 0; index < count; ++index) {
        const float share = static_cast<float>(index) / static_cast<float>(count - 1);
        const float t = index + 1 == count ? 1.0F : getParameterAtDistance(getLength() * share);
        frames[index].position = getPoint(t);
        frames[index].tangent = getTangent(t);
    }

    ParallelTransport::carry(frames, up, closed);
    return frames;
}

} // namespace haylen::math
