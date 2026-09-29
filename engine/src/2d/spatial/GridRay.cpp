#include "haylen/2d/spatial/GridRay.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

#include "haylen/math/Raycast.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::spatial2d {

void GridRay::requireCellSize(math::Vec2 cellSize) {
    if (!(cellSize.x > 0.0F) || !(cellSize.y > 0.0F) || !std::isfinite(cellSize.x) || !std::isfinite(cellSize.y)) {
        throw std::invalid_argument("Grid cells need a positive and finite size.");
    }
}

void GridRay::requireCastable(const math::Ray& ray, math::Vec2 cellSize) {
    requireCellSize(cellSize);
    if (!std::isfinite(ray.length)) {
        throw std::invalid_argument("A ray across an unbounded grid needs a finite length.");
    }
    const math::Vec2 end = ray.getEnd();
    for (const float cell : {ray.origin.x / cellSize.x, ray.origin.y / cellSize.y, end.x / cellSize.x, end.y / cellSize.y}) {
        if (!(std::fabs(cell) < kCellLimit)) {
            throw std::invalid_argument("A grid ray must stay within the 32-bit range of cells.");
        }
    }
}

std::optional<GridRay::Hit> GridRay::cast(const math::Ray& ray, math::Vec2 cellSize, const CellGrid& grid) {
    requireCellSize(cellSize);
    const math::Rect area{0.0F, 0.0F, static_cast<float>(grid.getWidth()) * cellSize.x, static_cast<float>(grid.getHeight()) * cellSize.y};
    const std::optional<math::RayHit> entry = math::Raycast::rect(ray, area);
    const std::optional<std::array<float, 2>> travel = math::Raycast::clip(ray, area);
    if (!entry || !travel) {
        return std::nullopt;
    }

    // The ray only travels between the sides of the grid where it enters and leaves it.
    const math::Ray inside{entry->point, ray.direction, (*travel)[1] - (*travel)[0]};
    std::optional<Hit> hit = cast(inside, cellSize, [&grid](Cell cell) { return grid.contains(cell) && grid[cell] != 0; });
    if (!hit) {
        return std::nullopt;
    }
    hit->distance += entry->distance;
    if (hit->normal.isZero()) {
        hit->normal = entry->normal;
    }
    return hit;
}

} // namespace haylen::spatial2d
