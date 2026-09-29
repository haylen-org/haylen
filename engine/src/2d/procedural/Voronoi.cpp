#include "haylen/2d/procedural/Voronoi.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "haylen/2d/procedural/Delaunay.hpp"
#include "haylen/math/Geometry.hpp"

namespace haylen::procedural2d {

// Each cell is the rectangle cut by the bisector with every Delaunay neighbor, because only neighbors in the triangulation share an edge of the diagram.
Voronoi::Voronoi(std::vector<math::Vec2> sites, const math::Rect& area) {
    if (!std::isfinite(area.getLeft()) || !std::isfinite(area.getTop()) || !std::isfinite(area.getRight()) || !std::isfinite(area.getBottom())) {
        throw std::invalid_argument("A Voronoi diagram needs a finite area.");
    }
    const Delaunay delaunay(std::move(sites));
    const std::vector<math::Vec2>& points = delaunay.getPoints();
    const std::vector<math::Vec2> corners{area.getMin(), {area.getRight(), area.getTop()}, area.getMax(), {area.getLeft(), area.getBottom()}};
    const bool single = delaunay.getHull().size() == 1;

    cells.resize(points.size());
    for (std::size_t index = 0; index < points.size(); ++index) {
        const std::vector<std::uint32_t>& neighbors = delaunay.getNeighbors()[index];
        if (neighbors.empty() && !(single && delaunay.getHull().front() == index)) {
            continue;
        }

        std::vector<math::Vec2> cell = corners;
        for (const std::uint32_t neighbor : neighbors) {
            cell = clip(cell, points[index], points[neighbor]);
            if (cell.empty()) {
                break;
            }
        }
        cells[index] = std::move(cell);
    }
}

std::vector<math::Vec2> Voronoi::clip(const std::vector<math::Vec2>& outline, math::Vec2 site, math::Vec2 other) {
    const math::Vec2 middle = (site + other) * 0.5F;
    const math::Vec2 normal = other - site;
    std::vector<math::Vec2> result;
    result.reserve(outline.size() + 1);

    for (std::size_t index = 0; index < outline.size(); ++index) {
        const math::Vec2 from = outline[index];
        const math::Vec2 to = outline[(index + 1) % outline.size()];
        const float fromSide = math::Vec2::dot(from - middle, normal);
        const float toSide = math::Vec2::dot(to - middle, normal);
        if (fromSide <= 0.0F) {
            result.push_back(from);
        }
        if ((fromSide <= 0.0F) != (toSide <= 0.0F)) {
            result.push_back(from + (to - from) * (fromSide / (fromSide - toSide)));
        }
    }
    return result.size() >= 3 ? result : std::vector<math::Vec2>{};
}

std::vector<math::Vec2> Voronoi::relax(std::vector<math::Vec2> sites, const math::Rect& area, int iterations) {
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const Voronoi diagram(sites, area);
        for (std::size_t index = 0; index < sites.size(); ++index) {
            const std::vector<math::Vec2>& cell = diagram.getCells()[index];
            if (!cell.empty()) {
                sites[index] = math::Geometry::centroid(cell);
            }
        }
    }
    return sites;
}

} // namespace haylen::procedural2d
