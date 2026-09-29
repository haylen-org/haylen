#include "haylen/2d/procedural/Region.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "haylen/2d/tiled/Object.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Polygon.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::procedural2d {

Region Region::rect(const math::Rect& value) {
    Region region;
    region.kind = Kind::Rect;
    region.bounds = value;
    region.area = std::max(0.0F, value.getArea());
    return region;
}

Region Region::circle(math::Vec2 middle, float radius) {
    return ring(middle, 0.0F, radius);
}

Region Region::ring(math::Vec2 middle, float inner, float outer) {
    if (inner < 0.0F || inner > outer) {
        throw std::invalid_argument("A ring needs an inner radius between zero and its outer radius.");
    }
    Region region;
    region.kind = inner > 0.0F ? Kind::Ring : Kind::Circle;
    region.center = middle;
    region.innerRadius = inner;
    region.outerRadius = outer;
    region.bounds = math::Circle{middle, outer}.getBounds();
    region.area = math::Math::kPi * (outer * outer - inner * inner);
    return region;
}

// Sampling picks a triangle by its share of the area, so the polygon is split into triangles once.
Region Region::polygon(std::span<const std::vector<math::Vec2>> outlines) {
    Region region;
    region.kind = Kind::Polygon;
    region.shape = math::Polygon::unite(outlines);
    for (const std::vector<math::Vec2>& triangle : math::Polygon::decompose(region.shape, 3)) {
        const float triangleArea = std::fabs(math::Geometry::signedArea(triangle));
        region.area += triangleArea;
        region.triangles.push_back({triangle[0], triangle[1], triangle[2]});
        region.cumulativeAreas.push_back(region.area);
    }
    if (region.triangles.empty()) {
        throw std::invalid_argument("A polygon region needs an outline with an area.");
    }

    std::vector<math::Vec2> corners;
    for (const std::vector<math::Vec2>& outline : region.shape) {
        corners.insert(corners.end(), outline.begin(), outline.end());
    }
    region.bounds = math::Geometry::bounds(corners);
    return region;
}

Region Region::fromObject(const tiled::Object& object) {
    const float width = object.size.x;
    const float height = object.size.y;
    std::vector<math::Vec2> outline;

    switch (object.shape) {
    case tiled::Object::Shape::Rectangle:
        if (object.rotation == 0.0F) {
            return rect({object.position.x, object.position.y, width, height});
        }
        outline = {{0.0F, 0.0F}, {width, 0.0F}, {width, height}, {0.0F, height}};
        break;
    case tiled::Object::Shape::Ellipse:
        if (object.rotation == 0.0F && width == height) {
            return circle(object.position + math::Vec2{width, height} * 0.5F, width * 0.5F);
        }
        for (int segment = 0; segment < kEllipseSegments; ++segment) {
            const float angle = math::Math::kTau * static_cast<float>(segment) / static_cast<float>(kEllipseSegments);
            outline.push_back({width * 0.5F * (1.0F + std::cos(angle)), height * 0.5F * (1.0F + std::sin(angle))});
        }
        break;
    case tiled::Object::Shape::Polygon:
        outline = object.points;
        break;
    default:
        throw std::invalid_argument("Only rectangle, ellipse and polygon objects have an area.");
    }

    // Tiled rotates objects around their position.
    for (math::Vec2& point : outline) {
        point = object.position + point.rotated(object.rotation);
    }
    const std::vector<std::vector<math::Vec2>> outlines{std::move(outline)};
    return polygon(outlines);
}

bool Region::contains(math::Vec2 point) const noexcept {
    switch (kind) {
    case Kind::Rect:
        return bounds.contains(point);
    case Kind::Circle:
    case Kind::Ring: {
        const float distanceSquared = math::Vec2::distanceSquared(point, center);
        return distanceSquared <= outerRadius * outerRadius && distanceSquared >= innerRadius * innerRadius;
    }
    case Kind::Polygon:
        break;
    }

    // Normalized outlines never cross, so a point inside an odd number of them is inside the area.
    bool inside = false;
    for (const std::vector<math::Vec2>& outline : shape) {
        if (math::Geometry::contains(outline, point)) {
            inside = !inside;
        }
    }
    return inside;
}

math::Vec2 Region::getRandomPoint(math::Random& random) const noexcept {
    switch (kind) {
    case Kind::Rect:
        return {random.range(bounds.getLeft(), bounds.getRight()), random.range(bounds.getTop(), bounds.getBottom())};
    case Kind::Circle:
    case Kind::Ring: {
        // The square root spreads points evenly over the area instead of crowding the center.
        const float inner = innerRadius * innerRadius;
        const float radius = std::sqrt(random.range(inner, outerRadius * outerRadius));
        return center + math::Vec2::fromAngle(random.range(0.0F, math::Math::kTau), radius);
    }
    case Kind::Polygon:
        break;
    }
    return getRandomPolygonPoint(random);
}

math::Vec2 Region::getRandomPolygonPoint(math::Random& random) const noexcept {
    const float target = random.range(0.0F, area);
    const auto found = std::upper_bound(cumulativeAreas.begin(), cumulativeAreas.end(), target);
    const auto index = std::min(static_cast<std::size_t>(found - cumulativeAreas.begin()), triangles.size() - 1);
    const std::array<math::Vec2, 3>& triangle = triangles[index];

    const float spread = std::sqrt(random.nextFloat());
    const float along = random.nextFloat();
    return triangle[0] * (1.0F - spread) + triangle[1] * (spread * (1.0F - along)) + triangle[2] * (spread * along);
}

} // namespace haylen::procedural2d
