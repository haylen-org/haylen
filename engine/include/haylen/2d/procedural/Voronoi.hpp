#pragma once

#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::procedural2d {

// The Voronoi diagram of a point set inside a rectangle, built from the Delaunay triangulation of the points. Cell i holds every position closer to point i than to any other point, as a convex outline wound with a positive signed area, so Delaunay::findNearest finds the cell that holds a position. Duplicate points get empty cells.
class Voronoi final {
  public:
    // Throws std::invalid_argument when a point or the area is not finite.
    Voronoi(std::vector<math::Vec2> sites, const math::Rect& area);

    [[nodiscard]] const std::vector<std::vector<math::Vec2>>& getCells() const noexcept {
        return cells;
    }

    // Moves every point to the centroid of its cell, iterations times, which evens out the spacing while keeping the points random. This is Lloyd relaxation.
    [[nodiscard]] static std::vector<math::Vec2> relax(std::vector<math::Vec2> sites, const math::Rect& area, int iterations);

  private:
    // Keeps the part of a convex outline that is at least as close to site as to other.
    [[nodiscard]] static std::vector<math::Vec2> clip(const std::vector<math::Vec2>& outline, math::Vec2 site, math::Vec2 other);

    std::vector<std::vector<math::Vec2>> cells;
};

} // namespace haylen::procedural2d
