#include "haylen/3d/spatial/GridRay.hpp"

#include <cmath>
#include <stdexcept>

namespace haylen::spatial3d {

void GridRay::requireCastable(const math::Ray3D& ray, math::Vec3 cellSize) {
    if (!(cellSize.x > 0.0F) || !(cellSize.y > 0.0F) || !(cellSize.z > 0.0F) || !cellSize.isFinite()) {
        throw std::invalid_argument("Grid cells need a positive and finite size.");
    }

    if (!std::isfinite(ray.length) || !ray.origin.isFinite() || !ray.direction.isFinite()) {
        throw std::invalid_argument("A ray across an unbounded grid needs a finite origin, direction and length.");
    }

    const math::Vec3 end = ray.getEndPoint();
    for (const float cell : {ray.origin.x / cellSize.x, ray.origin.y / cellSize.y, ray.origin.z / cellSize.z, end.x / cellSize.x, end.y / cellSize.y, end.z / cellSize.z}) {
        if (!(std::fabs(cell) < kCellLimit)) {
            throw std::invalid_argument("A grid ray must stay within the 32-bit range of cells.");
        }
    }
}

} // namespace haylen::spatial3d
