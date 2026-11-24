#include "haylen/math/Spline2D.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "math/ArcLength.hpp"

namespace haylen::math {

const std::array<std::pair<std::string_view, Spline2D::Kind>, 3> Spline2D::kKindNames{{{"catmullRom", Kind::CatmullRom}, {"bezier", Kind::Bezier}, {"bSpline", Kind::BSpline}}};

Spline2D::Spline2D(std::vector<Vec2> controlPoints, Kind curveKind, bool loop) : points(std::move(controlPoints)), kind(curveKind), closed(loop), segmentCount(countSegments(points.size(), curveKind, loop)) {
    measure();
}

std::optional<Spline2D::Kind> Spline2D::kindFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kKindNames, name, &std::pair<std::string_view, Kind>::first);
    return found != kKindNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Spline2D::kindName(Kind value) noexcept {
    return std::ranges::find(kKindNames, value, &std::pair<std::string_view, Kind>::second)->first;
}

std::size_t Spline2D::countSegments(std::size_t pointCount, Kind curveKind, bool loop) {
    switch (curveKind) {
    case Kind::Bezier:
        if (loop && pointCount >= 3 && pointCount % 3 == 0) {
            return pointCount / 3;
        }

        if (!loop && pointCount >= 4 && (pointCount - 1) % 3 == 0) {
            return (pointCount - 1) / 3;
        }

        throw std::invalid_argument("A Bezier spline needs 3n + 1 points when open and 3n points when closed.");
    case Kind::BSpline:
        if (pointCount < (loop ? 3U : 2U)) {
            throw std::invalid_argument("A B-spline needs at least two points when open and three when closed.");
        }

        // Open B-splines repeat their end points three times so the curve reaches them, which adds two segments.
        return loop ? pointCount : pointCount + 1;
    case Kind::CatmullRom:
        break;
    }
    if (pointCount < (loop ? 3U : 2U)) {
        throw std::invalid_argument("A Catmull-Rom spline needs at least two points when open and three when closed.");
    }

    return loop ? pointCount : pointCount - 1;
}

// Closed curves wrap indices around and open curves clamp them to the end points.
Vec2 Spline2D::controlPoint(std::ptrdiff_t index) const noexcept {
    const auto count = static_cast<std::ptrdiff_t>(points.size());
    if (closed) {
        return points[static_cast<std::size_t>(((index % count) + count) % count)];
    }

    return points[static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(index, 0, count - 1))];
}

Vec2 Spline2D::evaluate(std::size_t segment, float u) const noexcept {
    switch (kind) {
    case Kind::Bezier:
        return bezier(segment, u);
    case Kind::BSpline:
        return bSpline(segment, u);
    case Kind::CatmullRom:
        break;
    }
    return catmullRom(segment, u);
}

// Evaluates the centripetal curve with the Barry and Goldman pyramid. Open curves extend their ends by mirroring the neighboring point.
Vec2 Spline2D::catmullRom(std::size_t segment, float u) const noexcept {
    const auto index = static_cast<std::ptrdiff_t>(segment);
    const Vec2 p1 = controlPoint(index);
    const Vec2 p2 = controlPoint(index + 1);
    const Vec2 p0 = !closed && index == 0 ? p1 * 2.0F - p2 : controlPoint(index - 1);
    const Vec2 p3 = !closed && segment + 1 == segmentCount ? p2 * 2.0F - p1 : controlPoint(index + 2);

    const auto knot = [](Vec2 from, Vec2 to) { return std::max(std::pow(Vec2::distance(from, to), kAlpha), 1e-4F); };
    const float t0 = 0.0F;
    const float t1 = t0 + knot(p0, p1);
    const float t2 = t1 + knot(p1, p2);
    const float t3 = t2 + knot(p2, p3);
    const float t = t1 + (t2 - t1) * u;

    const Vec2 a1 = p0 * ((t1 - t) / (t1 - t0)) + p1 * ((t - t0) / (t1 - t0));
    const Vec2 a2 = p1 * ((t2 - t) / (t2 - t1)) + p2 * ((t - t1) / (t2 - t1));
    const Vec2 a3 = p2 * ((t3 - t) / (t3 - t2)) + p3 * ((t - t2) / (t3 - t2));
    const Vec2 b1 = a1 * ((t2 - t) / (t2 - t0)) + a2 * ((t - t0) / (t2 - t0));
    const Vec2 b2 = a2 * ((t3 - t) / (t3 - t1)) + a3 * ((t - t1) / (t3 - t1));
    return b1 * ((t2 - t) / (t2 - t1)) + b2 * ((t - t1) / (t2 - t1));
}

Vec2 Spline2D::bezier(std::size_t segment, float u) const noexcept {
    const auto first = static_cast<std::ptrdiff_t>(segment * 3);
    const float inverse = 1.0F - u;
    return controlPoint(first) * (inverse * inverse * inverse) + controlPoint(first + 1) * (3.0F * inverse * inverse * u) + controlPoint(first + 2) * (3.0F * inverse * u * u) + controlPoint(first + 3) * (u * u * u);
}

Vec2 Spline2D::bSpline(std::size_t segment, float u) const noexcept {
    const auto index = static_cast<std::ptrdiff_t>(segment) - 2;
    const float u2 = u * u;
    const float u3 = u2 * u;
    const float w0 = (1.0F - 3.0F * u + 3.0F * u2 - u3) / 6.0F;
    const float w1 = (4.0F - 6.0F * u2 + 3.0F * u3) / 6.0F;
    const float w2 = (1.0F + 3.0F * u + 3.0F * u2 - 3.0F * u3) / 6.0F;
    const float w3 = u3 / 6.0F;
    return controlPoint(index) * w0 + controlPoint(index + 1) * w1 + controlPoint(index + 2) * w2 + controlPoint(index + 3) * w3;
}

// Records the arc length at evenly spaced parameters, which distance lookups interpolate.
void Spline2D::measure() {
    lengths.assign(segmentCount * kStepsPerSegment + 1, 0.0F);
    Vec2 previous = evaluate(0, 0.0F);
    for (std::size_t segment = 0; segment < segmentCount; ++segment) {
        for (std::size_t step = 1; step <= kStepsPerSegment; ++step) {
            const Vec2 point = evaluate(segment, static_cast<float>(step) / static_cast<float>(kStepsPerSegment));
            const std::size_t index = segment * kStepsPerSegment + step;
            lengths[index] = lengths[index - 1] + Vec2::distance(previous, point);
            previous = point;
        }
    }
}

Vec2 Spline2D::getPoint(float t) const noexcept {
    const float wrapped = closed ? t - std::floor(t) : std::clamp(t, 0.0F, 1.0F);
    const float scaled = wrapped * static_cast<float>(segmentCount);

    // A parameter that is not a number lands in the first segment, whose point is not a number either.
    const std::size_t segment = scaled > 0.0F ? std::min(static_cast<std::size_t>(scaled), segmentCount - 1) : 0;
    return evaluate(segment, scaled - static_cast<float>(segment));
}

Vec2 Spline2D::getTangent(float t) const {
    const float step = 1e-3F / static_cast<float>(segmentCount);
    const float before = closed ? t - step : std::max(0.0F, t - step);
    const float after = closed ? t + step : std::min(1.0F, t + step);
    const Vec2 travel = getPoint(after) - getPoint(before);
    if (travel.isZero()) {
        throw std::invalid_argument("The spline stands still at this parameter, so it has no direction.");
    }

    return travel.getNormalized();
}

float Spline2D::getParameterAtDistance(float distance) const noexcept {
    return ArcLength::getParameter(lengths, distance, closed);
}

Vec2 Spline2D::getPointAtDistance(float distance) const noexcept {
    return getPoint(getParameterAtDistance(distance));
}

Vec2 Spline2D::getTangentAtDistance(float distance) const {
    return getTangent(getParameterAtDistance(distance));
}

std::vector<Vec2> Spline2D::sample(std::size_t count) const {
    ArcLength::checkCount(count);

    std::vector<Vec2> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(getPoint(count == 1 ? 0.0F : static_cast<float>(index) / static_cast<float>(count - 1)));
    }

    return result;
}

std::vector<Vec2> Spline2D::sampleByDistance(float spacing) const {
    const std::size_t count = ArcLength::countSteps(getLength(), spacing);
    std::vector<Vec2> result;
    result.reserve(count + 1);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(getPointAtDistance(static_cast<float>(index) * spacing));
    }

    if (!closed) {
        result.push_back(getPoint(1.0F));
    }

    return result;
}

void Spline2D::flatten(float tolerance, std::vector<Vec2>& target) const {
    if (!std::isfinite(tolerance) || tolerance <= 0.0F) {
        throw std::invalid_argument("Flattening a spline needs a finite tolerance above 0.");
    }

    target.clear();
    target.push_back(evaluate(0, 0.0F));
    for (std::size_t segment = 0; segment < segmentCount; ++segment) {
        subdivide(segment, 0.0F, 1.0F, evaluate(segment, 0.0F), evaluate(segment, 1.0F), tolerance, 0, target);
    }

    if (closed) {
        target.pop_back();
    }
}

// A piece is flat enough once the curve at its middle lies within the tolerance of its chord, and then only its end joins the points.
void Spline2D::subdivide(std::size_t segment, float from, float to, Vec2 start, Vec2 end, float tolerance, int depth, std::vector<Vec2>& target) const {
    const float middle = (from + to) * 0.5F;
    const Vec2 point = evaluate(segment, middle);
    const Vec2 chord = end - start;
    const float length = chord.getLength();
    const float deviation = length > 0.0F ? std::fabs(Vec2::cross(point - start, chord)) / length : Vec2::distance(point, start);
    if (depth >= kFlattenMaximumDepth || (depth >= kFlattenMinimumDepth && deviation <= tolerance)) {
        target.push_back(end);
        return;
    }

    subdivide(segment, from, middle, start, point, tolerance, depth + 1, target);
    subdivide(segment, middle, to, point, end, tolerance, depth + 1, target);
}

} // namespace haylen::math
