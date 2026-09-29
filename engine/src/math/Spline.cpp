#include "haylen/math/Spline.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace haylen::math {

Spline::Spline(std::vector<Vec2> controlPoints, Kind curveKind, bool loop) : points(std::move(controlPoints)), kind(curveKind), closed(loop), segmentCount(countSegments(points.size(), curveKind, loop)) {
    measure();
}

std::optional<Spline::Kind> Spline::kindFromName(std::string_view name) noexcept {
    if (name == "catmullRom") {
        return Kind::CatmullRom;
    }
    if (name == "bezier") {
        return Kind::Bezier;
    }
    if (name == "bspline") {
        return Kind::BSpline;
    }
    return std::nullopt;
}

std::string_view Spline::kindName(Kind value) noexcept {
    switch (value) {
    case Kind::Bezier:
        return "bezier";
    case Kind::BSpline:
        return "bspline";
    case Kind::CatmullRom:
        break;
    }
    return "catmullRom";
}

std::size_t Spline::countSegments(std::size_t pointCount, Kind curveKind, bool loop) {
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
Vec2 Spline::controlPoint(std::ptrdiff_t index) const noexcept {
    const auto count = static_cast<std::ptrdiff_t>(points.size());
    if (closed) {
        return points[static_cast<std::size_t>(((index % count) + count) % count)];
    }
    return points[static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(index, 0, count - 1))];
}

Vec2 Spline::evaluate(std::size_t segment, float u) const noexcept {
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
Vec2 Spline::catmullRom(std::size_t segment, float u) const noexcept {
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

Vec2 Spline::bezier(std::size_t segment, float u) const noexcept {
    const auto first = static_cast<std::ptrdiff_t>(segment * 3);
    const float inverse = 1.0F - u;
    return controlPoint(first) * (inverse * inverse * inverse) + controlPoint(first + 1) * (3.0F * inverse * inverse * u) + controlPoint(first + 2) * (3.0F * inverse * u * u) + controlPoint(first + 3) * (u * u * u);
}

Vec2 Spline::bSpline(std::size_t segment, float u) const noexcept {
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
void Spline::measure() {
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

Vec2 Spline::getPoint(float t) const noexcept {
    const float wrapped = closed ? t - std::floor(t) : std::clamp(t, 0.0F, 1.0F);
    const float scaled = wrapped * static_cast<float>(segmentCount);
    const std::size_t segment = std::min(static_cast<std::size_t>(scaled), segmentCount - 1);
    return evaluate(segment, scaled - static_cast<float>(segment));
}

Vec2 Spline::getTangent(float t) const noexcept {
    const float step = 1e-3F / static_cast<float>(segmentCount);
    const float before = closed ? t - step : std::max(0.0F, t - step);
    const float after = closed ? t + step : std::min(1.0F, t + step);
    return (getPoint(after) - getPoint(before)).getNormalized();
}

float Spline::getParameterAtDistance(float distance) const noexcept {
    const float length = getLength();
    if (length <= 0.0F) {
        return 0.0F;
    }

    const float along = closed ? distance - std::floor(distance / length) * length : std::clamp(distance, 0.0F, length);
    const auto upper = std::upper_bound(lengths.begin(), lengths.end(), along);
    if (upper == lengths.end()) {
        return 1.0F;
    }

    const auto index = static_cast<std::size_t>(upper - lengths.begin());
    const float start = lengths[index - 1];
    const float span = lengths[index] - start;
    const float fraction = span > 0.0F ? (along - start) / span : 0.0F;
    return (static_cast<float>(index - 1) + fraction) / static_cast<float>(lengths.size() - 1);
}

Vec2 Spline::getPointAtDistance(float distance) const noexcept {
    return getPoint(getParameterAtDistance(distance));
}

Vec2 Spline::getTangentAtDistance(float distance) const noexcept {
    return getTangent(getParameterAtDistance(distance));
}

std::vector<Vec2> Spline::sample(std::size_t count) const {
    std::vector<Vec2> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(getPoint(count == 1 ? 0.0F : static_cast<float>(index) / static_cast<float>(count - 1)));
    }
    return result;
}

std::vector<Vec2> Spline::sampleByDistance(float spacing) const {
    if (spacing <= 0.0F) {
        throw std::invalid_argument("Sampling by distance needs a positive spacing.");
    }

    const float length = getLength();
    const auto count = static_cast<std::size_t>(std::ceil(length / spacing));
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

} // namespace haylen::math
