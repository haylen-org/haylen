#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "haylen/math/Vec2.hpp"

namespace haylen::procedural2d {

// The Delaunay triangulation of a point set, built with a sweep hull in O(n log n). No point lies inside the circumcircle of any triangle. Duplicate points stay out of the triangulation, and points that all lie on one line give no triangles.
class Delaunay final {
  public:
    // Throws `std::invalid_argument` when a point is not finite.
    explicit Delaunay(std::vector<math::Vec2> sites);

    [[nodiscard]] const std::vector<math::Vec2>& getPoints() const noexcept {
        return points;
    }
    // Returns three point indices per triangle, wound with a positive `Geometry::signedArea`.
    [[nodiscard]] const std::vector<std::uint32_t>& getTriangles() const noexcept {
        return triangles;
    }
    // Returns, for each edge of each triangle, the index of the same edge in the neighboring triangle, or -1 on the convex hull. Edge `e` runs from point `triangles[e]` to the next point of its triangle.
    [[nodiscard]] const std::vector<std::int32_t>& getHalfedges() const noexcept {
        return halfedges;
    }
    // Returns the points on the convex hull in order around it, or all distinct points in order along the line when they are collinear.
    [[nodiscard]] const std::vector<std::uint32_t>& getHull() const noexcept {
        return hull;
    }
    // Returns the points joined to each point by an edge, in increasing order.
    [[nodiscard]] const std::vector<std::vector<std::uint32_t>>& getNeighbors() const noexcept {
        return neighbors;
    }
    [[nodiscard]] std::size_t getTriangleCount() const noexcept {
        return triangles.size() / 3;
    }

    // Returns the center of the circle through the corners of a triangle, whose index callers keep below `getTriangleCount`.
    [[nodiscard]] math::Vec2 getCircumcenter(std::size_t triangle) const noexcept;

    // Returns the index of the point nearest to the given position, which is also the Voronoi cell that holds it, by walking the edges from `start` in about the square root of the point count in steps. Returns `start` when it is a duplicate point without edges. Callers keep `start` below the point count, and the overload without `start` needs at least one point.
    [[nodiscard]] std::uint32_t findNearest(math::Vec2 position, std::uint32_t start) const noexcept;
    [[nodiscard]] std::uint32_t findNearest(math::Vec2 position) const noexcept;

  private:
    class Builder;

    void collectNeighbors();

    std::vector<math::Vec2> points;
    std::vector<std::uint32_t> triangles;
    std::vector<std::int32_t> halfedges;
    std::vector<std::uint32_t> hull;
    std::vector<std::vector<std::uint32_t>> neighbors;
};

} // namespace haylen::procedural2d
