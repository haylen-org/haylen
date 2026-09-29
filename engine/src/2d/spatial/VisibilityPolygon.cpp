#include "haylen/2d/spatial/VisibilityPolygon.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>

#include "haylen/math/Ray.hpp"
#include "haylen/math/RayHit.hpp"
#include "haylen/math/Raycast.hpp"

namespace haylen::spatial2d {

bool VisibilityPolygon::isFinite(math::Vec2 point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

const std::vector<math::Vec2>& VisibilityPolygon::compute(math::Vec2 origin, std::span<const math::Segment> walls, const math::Rect& bounds) {
    const bool finite = isFinite(bounds.getMin()) && isFinite(bounds.getMax()) && std::ranges::all_of(walls, [](const math::Segment& wall) { return isFinite(wall.start) && isFinite(wall.end); });
    if (!finite) {
        throw std::invalid_argument("A visibility polygon needs finite walls and bounds.");
    }
    if (!(origin.x > bounds.getLeft() && origin.x < bounds.getRight() && origin.y > bounds.getTop() && origin.y < bounds.getBottom())) {
        throw std::invalid_argument("A visibility polygon needs an origin inside its bounds.");
    }

    // The bounds close the view, so every ray from the origin ends on some segment.
    const std::array<math::Vec2, 4> corners{bounds.getMin(), math::Vec2{bounds.getRight(), bounds.getTop()}, bounds.getMax(), math::Vec2{bounds.getLeft(), bounds.getBottom()}};
    segments.assign(walls.begin(), walls.end());
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
        segments.push_back({corners[corner], corners[(corner + 1) % corners.size()]});
    }

    angles.clear();
    for (const math::Segment& segment : segments) {
        for (const math::Vec2 end : {segment.start, segment.end}) {
            const float angle = (end - origin).getAngle();
            angles.insert(angles.end(), {angle - kCornerAngle, angle, angle + kCornerAngle});
        }
    }
    for (float& angle : angles) {
        angle = std::remainder(angle, 2.0F * std::numbers::pi_v<float>);
    }
    std::ranges::sort(angles);
    angles.erase(std::ranges::unique(angles).begin(), angles.end());

    points.clear();
    for (const float angle : angles) {
        const std::optional<math::RayHit> hit = math::Raycast::segments(math::Ray::fromAngle(origin, angle, std::numeric_limits<float>::infinity()), segments);
        if (hit && (points.empty() || math::Vec2::distanceSquared(points.back(), hit->point) > 1e-6F)) {
            points.push_back(hit->point);
        }
    }
    return points;
}

} // namespace haylen::spatial2d
