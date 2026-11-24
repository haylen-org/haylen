#include "haylen/math/CurvePath3D.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>

#include "math/ArcLength.hpp"
#include "math/ParallelTransport.hpp"

namespace haylen::math {

const std::array<std::pair<std::string_view, CurvePath3D::Continuity>, 3> CurvePath3D::kContinuityNames{{{"position", Continuity::Position}, {"tangent", Continuity::Tangent}, {"velocity", Continuity::Velocity}}};

CurvePath3D::CurvePath3D(std::vector<Spline3D> pathCurves, bool loop) : curves(std::move(pathCurves)), closed(loop) {
    if (curves.empty()) {
        throw std::invalid_argument("A curve path needs at least one curve.");
    }

    starts.push_back(0.0F);
    for (std::size_t index = 0; index < curves.size(); ++index) {
        starts.push_back(starts.back() + curves[index].getLength());
        const std::size_t next = index + 1;
        if (next < curves.size()) {
            joins.push_back(measureJoin(curves[index], curves[next]));
        }
    }

    if (closed) {
        joins.push_back(measureJoin(curves.back(), curves.front()));
    }
}

std::optional<CurvePath3D::Continuity> CurvePath3D::continuityFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kContinuityNames, name, &std::pair<std::string_view, Continuity>::first);
    return found != kContinuityNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view CurvePath3D::continuityName(Continuity value) noexcept {
    return std::ranges::find(kContinuityNames, value, &std::pair<std::string_view, Continuity>::second)->first;
}

// Two curves join when the end of one lies on the start of the next, turn smoothly when their tangents agree there and move smoothly when their velocities agree too.
CurvePath3D::Continuity CurvePath3D::measureJoin(const Spline3D& before, const Spline3D& after) {
    const Vec3 end = before.getPoint(1.0F);
    const Vec3 start = after.getPoint(0.0F);
    const float scale = std::max({1.0F, end.getLength(), start.getLength()});
    if (Vec3::distance(end, start) > kJoinTolerance * scale) {
        throw std::invalid_argument(std::format("The curves of a path must join, but a curve starts {} away from the end of the curve before it.", Vec3::distance(end, start)));
    }

    const Vec3 leaving = before.getDerivatives(1.0F).first;
    const Vec3 arriving = after.getDerivatives(0.0F).first;
    if (leaving.isZero() || arriving.isZero() || Vec3::dot(leaving.getNormalized(), arriving.getNormalized()) < 1.0F - 1e-4F) {
        return Continuity::Position;
    }

    const float speed = std::max(leaving.getLength(), arriving.getLength());
    return Vec3::distance(leaving, arriving) <= 1e-3F * speed ? Continuity::Velocity : Continuity::Tangent;
}

float CurvePath3D::wrap(float distance) const noexcept {
    const float length = getLength();
    if (!closed || length <= 0.0F) {
        return std::clamp(distance, 0.0F, length);
    }

    return std::clamp(distance - std::floor(distance / length) * length, 0.0F, length);
}

// Returns the curve that holds a distance along the path and the distance along that curve.
std::pair<std::size_t, float> CurvePath3D::locate(float distance) const noexcept {
    const float along = wrap(distance);
    const auto upper = std::upper_bound(starts.begin() + 1, starts.end() - 1, along);
    const auto curve = static_cast<std::size_t>(upper - starts.begin()) - 1;
    return {curve, along - starts[curve]};
}

Vec3 CurvePath3D::getPointAtDistance(float distance) const noexcept {
    const auto [curve, along] = locate(distance);
    return curves[curve].getPointAtDistance(along);
}

Vec3 CurvePath3D::getTangentAtDistance(float distance) const {
    const auto [curve, along] = locate(distance);
    return curves[curve].getTangentAtDistance(along);
}

Vec3 CurvePath3D::getPoint(float t) const noexcept {
    return getPointAtDistance(t * getLength());
}

Vec3 CurvePath3D::getTangent(float t) const {
    return getTangentAtDistance(t * getLength());
}

CurvePath3D::ClosestPoint CurvePath3D::getClosestPoint(Vec3 point) const noexcept {
    ClosestPoint best{.distance = std::numeric_limits<float>::infinity()};
    for (std::size_t index = 0; index < curves.size(); ++index) {
        const Spline3D::ClosestPoint found = curves[index].getClosestPoint(point);
        if (found.distance < best.distance) {
            const float along = starts[index] + curves[index].getLength() * found.t;
            const float length = getLength();
            best = {.t = length > 0.0F ? along / length : 0.0F, .point = found.point, .distance = found.distance};
        }
    }

    return best;
}

std::vector<Vec3> CurvePath3D::sampleEvenly(std::size_t count) const {
    ArcLength::checkCount(count);
    std::vector<Vec3> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const float share = count == 1 ? 0.0F : static_cast<float>(index) / static_cast<float>(count - 1);
        result.push_back(share >= 1.0F ? curves.back().getPoint(1.0F) : getPointAtDistance(getLength() * share));
    }

    return result;
}

std::vector<Vec3> CurvePath3D::sampleByDistance(float spacing) const {
    const std::size_t count = ArcLength::countSteps(getLength(), spacing);
    std::vector<Vec3> result;
    result.reserve(count + 1);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(getPointAtDistance(static_cast<float>(index) * spacing));
    }

    if (!closed) {
        result.push_back(curves.back().getPoint(1.0F));
    }

    return result;
}

std::vector<CurveFrame> CurvePath3D::getFrames(std::size_t count, Vec3 up) const {
    if (count < 2) {
        throw std::invalid_argument("The frames of a curve need at least 2 frames, one at each end.");
    }

    ArcLength::checkCount(count);
    std::vector<CurveFrame> frames(count);
    for (std::size_t index = 0; index < count; ++index) {
        const bool last = index + 1 == count;
        const float distance = getLength() * static_cast<float>(index) / static_cast<float>(count - 1);
        frames[index].position = last && !closed ? curves.back().getPoint(1.0F) : getPointAtDistance(last ? 0.0F : distance);
        frames[index].tangent = last && !closed ? curves.back().getTangent(1.0F) : getTangentAtDistance(last ? 0.0F : distance);
    }

    ParallelTransport::carry(frames, up, closed);
    return frames;
}

} // namespace haylen::math
