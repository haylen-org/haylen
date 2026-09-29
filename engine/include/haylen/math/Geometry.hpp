#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "haylen/math/Circle.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Segment.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// Intersection tests and polygon algorithms on the math types.
class Geometry final {
  public:
    [[nodiscard]] static bool intersects(const Circle& lhs, const Circle& rhs) noexcept;
    [[nodiscard]] static bool intersects(const Circle& circle, const Rect& rect) noexcept;
    [[nodiscard]] static std::optional<Vec2> intersection(const Segment& lhs, const Segment& rhs) noexcept;
    [[nodiscard]] static Vec2 closestPoint(const Segment& segment, Vec2 point) noexcept;
    [[nodiscard]] static float distanceToSegment(const Segment& segment, Vec2 point) noexcept;

    [[nodiscard]] static bool contains(std::span<const Vec2> polygon, Vec2 point) noexcept;
    [[nodiscard]] static float signedArea(std::span<const Vec2> polygon) noexcept;
    [[nodiscard]] static Vec2 centroid(std::span<const Vec2> polygon) noexcept;
    [[nodiscard]] static bool isConvex(std::span<const Vec2> polygon) noexcept;
    [[nodiscard]] static Rect bounds(std::span<const Vec2> points) noexcept;
    [[nodiscard]] static std::vector<Vec2> convexHull(std::span<const Vec2> points);

    // Triangulates a simple polygon in either winding and returns counter-clockwise triangle indices.
    [[nodiscard]] static std::vector<std::uint32_t> triangulate(std::span<const Vec2> polygon);

  private:
    [[nodiscard]] static bool pointInTriangle(Vec2 point, Vec2 a, Vec2 b, Vec2 c) noexcept;
};

} // namespace haylen::math
