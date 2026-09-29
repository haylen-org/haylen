#include "haylen/math/Raycast.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "haylen/math/Geometry.hpp"

namespace haylen::math {

Vec2 Raycast::facingNormal(Vec2 edge, Vec2 direction) noexcept {
    const Vec2 normal = edge.getPerpendicular().getNormalized();
    return Vec2::dot(normal, direction) > 0.0F ? -normal : normal;
}

std::optional<float> Raycast::crossing(const Ray& ray, Vec2 start, Vec2 end) noexcept {
    const Vec2 edge = end - start;
    const float denominator = Vec2::cross(ray.direction, edge);
    if (std::fabs(denominator) <= 1e-9F) {
        return std::nullopt;
    }

    const Vec2 offset = start - ray.origin;
    const float distance = Vec2::cross(offset, edge) / denominator;
    const float along = Vec2::cross(offset, ray.direction) / denominator;
    if (distance < 0.0F || distance > ray.length || along < 0.0F || along > 1.0F) {
        return std::nullopt;
    }
    return distance;
}

std::optional<std::array<float, 2>> Raycast::clip(const Ray& ray, const Rect& area) noexcept {
    float enter = 0.0F;
    float exit = ray.length;
    for (float Vec2::* axis : {&Vec2::x, &Vec2::y}) {
        const float origin = ray.origin.*axis;
        const float direction = ray.direction.*axis;
        const float low = area.getMin().*axis;
        const float high = area.getMax().*axis;
        if (direction == 0.0F) {
            if (origin < low || origin > high) {
                return std::nullopt;
            }
            continue;
        }
        enter = std::max(enter, ((direction > 0.0F ? low : high) - origin) / direction);
        exit = std::min(exit, ((direction > 0.0F ? high : low) - origin) / direction);
    }

    if (enter > exit) {
        return std::nullopt;
    }
    return std::array<float, 2>{enter, exit};
}

std::optional<RayHit> Raycast::segment(const Ray& ray, const Segment& target) noexcept {
    const std::optional<float> distance = crossing(ray, target.start, target.end);
    if (!distance) {
        return std::nullopt;
    }
    return RayHit{.point = ray.at(*distance), .normal = facingNormal(target.end - target.start, ray.direction), .distance = *distance};
}

std::optional<RayHit> Raycast::rect(const Ray& ray, const Rect& target) noexcept {
    const Vec2 minimum = target.getMin();
    const Vec2 maximum = target.getMax();
    float enter = -std::numeric_limits<float>::infinity();
    float exit = std::numeric_limits<float>::infinity();
    Vec2 normal;

    // Clips the ray against the slab of each axis and keeps the side it enters last, whose normal is the one it hits.
    for (float Vec2::* axis : {&Vec2::x, &Vec2::y}) {
        const float origin = ray.origin.*axis;
        const float direction = ray.direction.*axis;
        if (direction == 0.0F) {
            if (origin < minimum.*axis || origin > maximum.*axis) {
                return std::nullopt;
            }
            continue;
        }

        const float near = ((direction > 0.0F ? minimum.*axis : maximum.*axis) - origin) / direction;
        const float far = ((direction > 0.0F ? maximum.*axis : minimum.*axis) - origin) / direction;
        if (near > enter) {
            enter = near;
            normal = {};
            normal.*axis = direction > 0.0F ? -1.0F : 1.0F;
        }
        exit = std::min(exit, far);
    }

    if (enter > exit || exit < 0.0F || enter > ray.length) {
        return std::nullopt;
    }
    if (enter < 0.0F) {
        return RayHit{.point = ray.origin};
    }
    return RayHit{.point = ray.at(enter), .normal = normal, .distance = enter};
}

std::optional<RayHit> Raycast::circle(const Ray& ray, const Circle& target) noexcept {
    const Vec2 offset = ray.origin - target.center;
    const float outside = offset.getLengthSquared() - target.radius * target.radius;
    if (outside <= 0.0F) {
        return RayHit{.point = ray.origin};
    }

    const float along = Vec2::dot(offset, ray.direction);
    const float discriminant = along * along - outside;
    if (along > 0.0F || discriminant < 0.0F) {
        return std::nullopt;
    }

    const float distance = -along - std::sqrt(discriminant);
    if (distance > ray.length) {
        return std::nullopt;
    }
    const Vec2 point = ray.at(distance);
    return RayHit{.point = point, .normal = (point - target.center).getNormalized(), .distance = distance};
}

std::optional<RayHit> Raycast::polygon(const Ray& ray, std::span<const Vec2> points) noexcept {
    if (points.size() < 3) {
        return std::nullopt;
    }
    if (Geometry::contains(points, ray.origin)) {
        return RayHit{.point = ray.origin};
    }
    return chain(ray, points, true);
}

std::optional<RayHit> Raycast::chain(const Ray& ray, std::span<const Vec2> points, bool loop) noexcept {
    if (points.size() < 2) {
        return std::nullopt;
    }

    const std::size_t edges = loop ? points.size() : points.size() - 1;
    std::optional<RayHit> closest;
    for (std::size_t index = 0; index < edges; ++index) {
        const Vec2 start = points[index];
        const Vec2 end = points[(index + 1) % points.size()];
        const std::optional<float> distance = crossing(ray, start, end);
        if (distance && (!closest || *distance < closest->distance)) {
            closest = RayHit{.point = ray.at(*distance), .normal = facingNormal(end - start, ray.direction), .distance = *distance, .index = index};
        }
    }
    return closest;
}

std::optional<RayHit> Raycast::segments(const Ray& ray, std::span<const Segment> targets) noexcept {
    std::optional<RayHit> closest;
    for (std::size_t index = 0; index < targets.size(); ++index) {
        const Segment& target = targets[index];
        const std::optional<float> distance = crossing(ray, target.start, target.end);
        if (distance && (!closest || *distance < closest->distance)) {
            closest = RayHit{.point = ray.at(*distance), .normal = facingNormal(target.end - target.start, ray.direction), .distance = *distance, .index = index};
        }
    }
    return closest;
}

void Raycast::segmentsAll(const Ray& ray, std::span<const Segment> targets, std::size_t limit, std::vector<RayHit>& hits) {
    hits.clear();
    for (std::size_t index = 0; index < targets.size(); ++index) {
        const Segment& target = targets[index];
        if (const std::optional<float> distance = crossing(ray, target.start, target.end)) {
            hits.push_back({.point = ray.at(*distance), .normal = facingNormal(target.end - target.start, ray.direction), .distance = *distance, .index = index});
        }
    }

    std::ranges::sort(hits, [](const RayHit& lhs, const RayHit& rhs) { return lhs.distance != rhs.distance ? lhs.distance < rhs.distance : lhs.index < rhs.index; });
    if (limit > 0 && hits.size() > limit) {
        hits.resize(limit);
    }
}

} // namespace haylen::math
