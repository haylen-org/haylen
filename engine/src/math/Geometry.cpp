#include "haylen/math/Geometry.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace haylen::math {

bool Geometry::pointInTriangle(Vec2 point, Vec2 a, Vec2 b, Vec2 c) noexcept {
    const float ab = Vec2::cross(b - a, point - a);
    const float bc = Vec2::cross(c - b, point - b);
    const float ca = Vec2::cross(a - c, point - c);
    return ab >= 0.0F && bc >= 0.0F && ca >= 0.0F;
}

bool Geometry::intersects(const Circle& lhs, const Circle& rhs) noexcept {
    const float radius = lhs.radius + rhs.radius;
    return Vec2::distanceSquared(lhs.center, rhs.center) <= radius * radius;
}

bool Geometry::intersects(const Circle& circle, const Rect& rect) noexcept {
    return Vec2::distanceSquared(rect.clamp(circle.center), circle.center) <= circle.radius * circle.radius;
}

std::optional<Vec2> Geometry::intersection(const Segment& lhs, const Segment& rhs) noexcept {
    const Vec2 r = lhs.end - lhs.start;
    const Vec2 s = rhs.end - rhs.start;
    const float denominator = Vec2::cross(r, s);
    if (std::fabs(denominator) <= 1e-9F) {
        return std::nullopt;
    }

    const Vec2 offset = rhs.start - lhs.start;
    const float t = Vec2::cross(offset, s) / denominator;
    const float u = Vec2::cross(offset, r) / denominator;
    if (t < 0.0F || t > 1.0F || u < 0.0F || u > 1.0F) {
        return std::nullopt;
    }
    return lhs.start + r * t;
}

Vec2 Geometry::closestPoint(const Segment& segment, Vec2 point) noexcept {
    const Vec2 direction = segment.end - segment.start;
    const float lengthSquared = direction.getLengthSquared();
    if (lengthSquared <= 0.0F) {
        return segment.start;
    }

    const float t = std::clamp(Vec2::dot(point - segment.start, direction) / lengthSquared, 0.0F, 1.0F);
    return segment.start + direction * t;
}

float Geometry::distanceToSegment(const Segment& segment, Vec2 point) noexcept {
    return Vec2::distance(closestPoint(segment, point), point);
}

bool Geometry::contains(std::span<const Vec2> polygon, Vec2 point) noexcept {
    bool inside = false;
    for (std::size_t current = 0, previous = polygon.size() - 1; current < polygon.size(); previous = current++) {
        const Vec2 a = polygon[current];
        const Vec2 b = polygon[previous];
        const bool crosses = (a.y > point.y) != (b.y > point.y);
        if (crosses && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x) {
            inside = !inside;
        }
    }
    return inside;
}

float Geometry::signedArea(std::span<const Vec2> polygon) noexcept {
    float area = 0.0F;
    for (std::size_t current = 0, previous = polygon.size() - 1; current < polygon.size(); previous = current++) {
        area += Vec2::cross(polygon[previous], polygon[current]);
    }
    return area * 0.5F;
}

Vec2 Geometry::centroid(std::span<const Vec2> polygon) noexcept {
    const float area = signedArea(polygon);
    if (std::fabs(area) <= 1e-9F) {
        const Vec2 sum = std::accumulate(polygon.begin(), polygon.end(), Vec2{});
        return polygon.empty() ? Vec2{} : sum / static_cast<float>(polygon.size());
    }

    Vec2 center{};
    for (std::size_t current = 0, previous = polygon.size() - 1; current < polygon.size(); previous = current++) {
        const float factor = Vec2::cross(polygon[previous], polygon[current]);
        center += (polygon[previous] + polygon[current]) * factor;
    }
    return center / (6.0F * area);
}

bool Geometry::isConvex(std::span<const Vec2> polygon) noexcept {
    if (polygon.size() < 3) {
        return false;
    }

    float winding = 0.0F;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        const Vec2 a = polygon[index];
        const Vec2 b = polygon[(index + 1) % polygon.size()];
        const Vec2 c = polygon[(index + 2) % polygon.size()];
        const float turn = Vec2::cross(b - a, c - b);
        if (turn == 0.0F) {
            continue;
        }
        if (winding != 0.0F && (turn > 0.0F) != (winding > 0.0F)) {
            return false;
        }
        winding = turn;
    }
    return winding != 0.0F;
}

Rect Geometry::bounds(std::span<const Vec2> points) noexcept {
    if (points.empty()) {
        return {};
    }

    Vec2 minimum = points.front();
    Vec2 maximum = points.front();
    for (const Vec2 point : points) {
        minimum = Vec2::min(minimum, point);
        maximum = Vec2::max(maximum, point);
    }
    return Rect::fromMinMax(minimum, maximum);
}

std::vector<Vec2> Geometry::convexHull(std::span<const Vec2> points) {
    std::vector<Vec2> sorted(points.begin(), points.end());
    // clang-format off
    std::sort(sorted.begin(), sorted.end(), [](Vec2 lhs, Vec2 rhs) {
        return lhs.x < rhs.x || (lhs.x == rhs.x && lhs.y < rhs.y);
    });
    // clang-format on
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    if (sorted.size() < 3) {
        return sorted;
    }

    // Andrew's monotone chain builds the lower hull, then the upper hull.
    std::vector<Vec2> hull(sorted.size() * 2);
    std::size_t count = 0;
    for (const Vec2 point : sorted) {
        while (count >= 2 && Vec2::cross(hull[count - 1] - hull[count - 2], point - hull[count - 2]) <= 0.0F) {
            --count;
        }
        hull[count++] = point;
    }

    const std::size_t lowerCount = count + 1;
    for (std::size_t index = sorted.size() - 1; index > 0; --index) {
        const Vec2 point = sorted[index - 1];
        while (count >= lowerCount && Vec2::cross(hull[count - 1] - hull[count - 2], point - hull[count - 2]) <= 0.0F) {
            --count;
        }
        hull[count++] = point;
    }

    hull.resize(count - 1);
    return hull;
}

std::vector<std::uint32_t> Geometry::triangulate(std::span<const Vec2> polygon) {
    std::vector<std::uint32_t> triangles;
    if (polygon.size() < 3) {
        return triangles;
    }

    std::vector<std::uint32_t> remaining(polygon.size());
    std::iota(remaining.begin(), remaining.end(), std::uint32_t{0});
    if (signedArea(polygon) < 0.0F) {
        std::reverse(remaining.begin(), remaining.end());
    }
    triangles.reserve((polygon.size() - 2) * 3);

    // Ear clipping removes one convex vertex whose triangle contains no other vertex per iteration.
    std::size_t guard = remaining.size() * remaining.size();
    std::size_t index = 0;
    while (remaining.size() > 3 && guard-- > 0) {
        const std::size_t count = remaining.size();
        const std::uint32_t previous = remaining[(index + count - 1) % count];
        const std::uint32_t current = remaining[index % count];
        const std::uint32_t next = remaining[(index + 1) % count];
        const Vec2 a = polygon[previous];
        const Vec2 b = polygon[current];
        const Vec2 c = polygon[next];

        bool ear = Vec2::cross(b - a, c - b) > 0.0F;
        for (std::size_t other = 0; ear && other < count; ++other) {
            const std::uint32_t vertex = remaining[other];
            if (vertex != previous && vertex != current && vertex != next && pointInTriangle(polygon[vertex], a, b, c)) {
                ear = false;
            }
        }

        if (!ear) {
            index = (index + 1) % count;
            continue;
        }

        triangles.insert(triangles.end(), {previous, current, next});
        remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(index % count));
        index = index % remaining.size();
    }

    if (remaining.size() == 3) {
        triangles.insert(triangles.end(), {remaining[0], remaining[1], remaining[2]});
    }
    return triangles;
}

} // namespace haylen::math
