#include "haylen/math/Spline3D.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "math/ArcLength.hpp"
#include "math/Curve3DHelper.hpp"
#include "math/NurbsBasis.hpp"

namespace haylen::math {

const std::array<std::pair<std::string_view, Spline3D::Kind>, 6> Spline3D::kKindNames{{{"linear", Kind::Linear}, {"catmullRom", Kind::CatmullRom}, {"quadraticBezier", Kind::QuadraticBezier}, {"bezier", Kind::Bezier}, {"bSpline", Kind::BSpline}, {"nurbs", Kind::Nurbs}}};

Spline3D::Spline3D(std::vector<Vec3> controlPoints) : Spline3D(std::move(controlPoints), Options{}) {}

Spline3D::Spline3D(std::vector<Vec3> controlPoints, const Options& options) : points(std::move(controlPoints)), kind(options.kind), closed(options.closed) {
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

std::optional<Spline3D::Kind> Spline3D::kindFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kKindNames, name, &std::pair<std::string_view, Kind>::first);
    return found != kKindNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Spline3D::kindName(Kind value) noexcept {
    return std::ranges::find(kKindNames, value, &std::pair<std::string_view, Kind>::second)->first;
}

// The ellipse is an affine image of the unit circle, which rational quadratic Bezier pieces of at most a quarter turn draw exactly: the middle point of each piece lies where the tangents at its ends cross, with the cosine of half its turn as its weight.
Spline3D Spline3D::ellipse(Vec3 center, Vec3 xAxis, Vec3 yAxis, float startAngle, float endAngle) {
    if (!center.isFinite() || !xAxis.isFinite() || !yAxis.isFinite() || !(Vec3::cross(xAxis, yAxis).getLength() > kFlatSine * xAxis.getLength() * yAxis.getLength())) {
        throw std::invalid_argument("The center and the two axes of an ellipse must be finite, and the axes must run in different directions so they span a plane.");
    }

    const double sweep = static_cast<double>(endAngle) - startAngle;
    if (!std::isfinite(startAngle) || !std::isfinite(endAngle) || sweep == 0.0 || std::fabs(sweep) > kWholeTurn * (1.0 + kTurnTolerance)) {
        throw std::invalid_argument("The angles of an ellipse must be finite and apart by more than 0 and at most a whole turn.");
    }

    const bool whole = std::fabs(sweep) >= kWholeTurn * (1.0 - kTurnTolerance);
    const auto pieces = static_cast<std::size_t>(std::max(1.0, std::ceil(std::fabs(sweep) / (kWholeTurn / 4.0) - kTurnTolerance)));
    const double step = sweep / static_cast<double>(pieces);
    const double weight = std::cos(step / 2.0);
    const auto at = [&](double angle, double reach) { return center + xAxis * static_cast<float>(std::cos(angle) * reach) + yAxis * static_cast<float>(std::sin(angle) * reach); };

    std::vector<Vec3> controlPoints{at(startAngle, 1.0)};
    Options options{.kind = Kind::Nurbs, .closed = whole, .weights = {1.0F}, .knots = {0.0F, 0.0F, 0.0F}, .degree = 2};
    for (std::size_t piece = 0; piece < pieces; ++piece) {
        const double from = startAngle + step * static_cast<double>(piece);
        const bool last = piece + 1 == pieces;
        controlPoints.push_back(at(from + step / 2.0, 1.0 / weight));
        controlPoints.push_back(last && whole ? controlPoints.front() : at(from + step, 1.0));
        options.weights.push_back(static_cast<float>(weight));
        options.weights.push_back(1.0F);
        options.knots.insert(options.knots.end(), last ? 3U : 2U, static_cast<float>(piece + 1));
    }

    return Spline3D(std::move(controlPoints), options);
}

// The start turns around the foot of its perpendicular on the axis, so the circle lies in the plane through the start across the normal.
Spline3D Spline3D::arc(Vec3 center, Vec3 start, Vec3 normal, float angle) {
    if (!center.isFinite() || !start.isFinite() || !normal.isFinite() || normal.isZero()) {
        throw std::invalid_argument("The center and the start of an arc must be finite, and its normal must have a finite length above 0.");
    }

    if (!std::isfinite(angle) || angle == 0.0F || std::fabs(angle) > Math::kTau * (1.0F + static_cast<float>(kTurnTolerance))) {
        throw std::invalid_argument("The angle of an arc must be a finite number of radians other than 0 and at most a whole turn.");
    }

    const Vec3 axis = normal.getNormalized();
    const Vec3 foot = center + axis * Vec3::dot(start - center, axis);
    const Vec3 xAxis = start - foot;
    if (!(xAxis.getLength() > kFlatSine * std::max(1.0F, (start - center).getLength()))) {
        throw std::invalid_argument("The start of an arc lies on its axis, so the arc has no radius.");
    }

    return ellipse(foot, xAxis, Vec3::cross(axis, xAxis), 0.0F, angle);
}

// The center of the circle through three points is the point of their plane as far from all three, and the arc turns from the start through the middle point by the right hand around the normal of the three.
Spline3D Spline3D::arcThrough(Vec3 start, Vec3 through, Vec3 end) {
    const Vec3 toThrough = through - start;
    const Vec3 toEnd = end - start;
    const Vec3 normal = Vec3::cross(toThrough, toEnd);
    if (!start.isFinite() || !through.isFinite() || !end.isFinite() || !(normal.getLength() > kFlatSine * toThrough.getLength() * toEnd.getLength())) {
        throw std::invalid_argument("The three points of an arc must be finite and must not lie on one line, or no circle passes through them.");
    }

    const Vec3 offset = (Vec3::cross(normal, toThrough) * toEnd.getLengthSquared() + Vec3::cross(toEnd, normal) * toThrough.getLengthSquared()) / (2.0F * normal.getLengthSquared());
    const Vec3 center = start + offset;
    const Vec3 xAxis = start - center;
    const Vec3 yAxis = Vec3::cross(normal.getNormalized(), xAxis);
    const Vec3 toFinish = end - center;
    const double angle = std::atan2(static_cast<double>(Vec3::dot(toFinish, yAxis)), static_cast<double>(Vec3::dot(toFinish, xAxis)));
    const double turned = angle > 0.0 ? angle : angle + kWholeTurn;
    return ellipse(center, xAxis, yAxis, 0.0F, static_cast<float>(turned));
}

// Closed curves wrap indices around and open curves clamp them to the end points.
Vec3 Spline3D::wrappedPoint(std::ptrdiff_t index) const noexcept {
    const auto count = static_cast<std::ptrdiff_t>(points.size());
    if (closed) {
        return points[static_cast<std::size_t>(((index % count) + count) % count)];
    }

    return points[static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(index, 0, count - 1))];
}

void Spline3D::buildLinear() {
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
void Spline3D::buildCatmullRom(float alpha, float tension) {
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

void Spline3D::buildBezier(int segmentDegree) {
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
void Spline3D::buildBSpline() {
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

void Spline3D::buildNurbs(const Options& options) {
    const std::size_t count = points.size();
    weights = NurbsBasis::checkWeights("NURBS spline", options.weights, count);
    degree = options.degree;
    const bool fitsDegree = degree >= 1 && degree <= NurbsBasis::kMaxDegree && count > static_cast<std::size_t>(degree);
    knots = options.knots.empty() && fitsDegree ? NurbsBasis::makeClampedKnots(count, degree) : options.knots;
    NurbsBasis::check("NURBS spline", count, degree, knots);
    controls = points;
    if (!closed) {
        return;
    }

    // The knots end where the curve ends, which a closed curve wraps around to its start.
    const Vec3 start = evaluate(knots[static_cast<std::size_t>(degree)]).point;
    const Vec3 end = evaluate(knots[controls.size()]).point;
    const float scale = std::max({1.0F, start.getLength(), end.getLength()});
    if (!(Vec3::distance(start, end) <= kCloseTolerance * scale)) {
        throw std::invalid_argument("A closed NURBS spline must end where it starts.");
    }
}

// The segments share their end points, so each one adds the points after its first and the knot of its end once for every degree.
void Spline3D::appendBezierSegment(std::span<const Vec3> segment) {
    if (controls.empty()) {
        controls.push_back(segment.front());
        knots.assign(segment.size(), 0.0F);
    }

    controls.insert(controls.end(), segment.begin() + 1, segment.end());
    const float knot = knots.back() + 1.0F;
    knots.insert(knots.end(), segment.size() - 1, knot);
}

void Spline3D::measure() {
    Curve3DHelper::measure(*this, segmentCount * kStepsPerSegment, lengths);
}

float Spline3D::toKnot(float t) const noexcept {
    const float low = knots[static_cast<std::size_t>(degree)];
    const float high = knots[controls.size()];
    const float wrapped = closed ? t - std::floor(t) : std::clamp(t, 0.0F, 1.0F);
    return low + (high - low) * wrapped;
}

// The point is the ratio of the weighted points to the weights, and its derivatives follow by the rule of quotients. A parameter that is not a number gives a point that is not a number either.
Spline3D::Derivatives Spline3D::evaluate(float u) const noexcept {
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

Vec3 Spline3D::getPoint(float t) const noexcept {
    return evaluate(toKnot(t)).point;
}

Spline3D::Derivatives Spline3D::getDerivatives(float t) const noexcept {
    return evaluate(toKnot(t));
}

Vec3 Spline3D::getTangent(float t) const {
    const std::optional<Vec3> direction = Curve3DHelper::getDirection(getDerivatives(t));
    if (!direction) {
        throw std::invalid_argument("The spline stands still at this parameter, so it has no direction.");
    }

    return *direction;
}

float Spline3D::getParameterAtDistance(float distance) const noexcept {
    return ArcLength::getParameter(lengths, distance, closed);
}

Vec3 Spline3D::getPointAtDistance(float distance) const noexcept {
    return getPoint(getParameterAtDistance(distance));
}

Vec3 Spline3D::getTangentAtDistance(float distance) const {
    return getTangent(getParameterAtDistance(distance));
}

Spline3D::ClosestPoint Spline3D::getClosestPoint(Vec3 point) const noexcept {
    return Curve3DHelper::findClosest(*this, point, lengths.size() - 1);
}

std::vector<Vec3> Spline3D::sample(std::size_t count) const {
    return Curve3DHelper::sample(*this, count);
}

std::vector<Vec3> Spline3D::sampleEvenly(std::size_t count) const {
    return Curve3DHelper::sampleEvenly(*this, count);
}

std::vector<Vec3> Spline3D::sampleByDistance(float spacing) const {
    return Curve3DHelper::sampleByDistance(*this, spacing);
}

std::vector<CurveFrame> Spline3D::getFrames(std::size_t count, Vec3 up) const {
    return Curve3DHelper::getFrames(*this, count, up);
}

} // namespace haylen::math
