#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

#include "haylen/3d/spatial/Cell.hpp"
#include "haylen/math/Ray3D.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::spatial3d {

// Walks the cells that a ray crosses in a grid of equal boxes whose first cell starts at the world origin, with the digital differential analyzer of Amanatides and Woo, such as the voxels of a block world. A ray through an exact edge or corner of cells steps along x first, then y.
class GridRay final {
  public:
    // Calls `visit(cell, distance, normal)` for every crossed cell in order, with the distance where the ray enters the cell and the normal of the face it enters through, which is zero for the cell of the origin. Visiting stops when `visit` returns `false` or the ray ends, so the ray needs a finite length within the 32-bit range of cells and the cells a positive size, or it throws `std::invalid_argument`.
    template <typename Visit> static void traverse(const math::Ray3D& ray, math::Vec3 cellSize, Visit&& visit) {
        requireCastable(ray, cellSize);
        const std::array<float, 3> origin{ray.origin.x, ray.origin.y, ray.origin.z};
        const std::array<float, 3> direction{ray.direction.x, ray.direction.y, ray.direction.z};
        const std::array<float, 3> size{cellSize.x, cellSize.y, cellSize.z};
        std::array<int, 3> cell{};
        std::array<int, 3> step{};

        // The distances to the next boundaries add up in double precision, which keeps them growing across every cell of the 32-bit range where a float stops changing after about 2^24 steps.
        std::array<double, 3> next{};
        std::array<double, 3> delta{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            cell[axis] = static_cast<int>(std::floor(origin[axis] / size[axis]));
            step[axis] = direction[axis] > 0.0F ? 1 : -1;
            next[axis] = boundary(origin[axis], direction[axis], size[axis], cell[axis]);
            delta[axis] = direction[axis] != 0.0F ? static_cast<double>(size[axis]) / std::fabs(static_cast<double>(direction[axis])) : kUnreachable;
        }

        if (!visit(Cell{cell[0], cell[1], cell[2]}, 0.0F, math::Vec3{})) {
            return;
        }

        for (;;) {
            const std::size_t axis = next[0] <= next[1] && next[0] <= next[2] ? 0 : (next[1] <= next[2] ? 1 : 2);
            const double distance = next[axis];
            cell[axis] += step[axis];
            next[axis] += delta[axis];
            std::array<float, 3> normal{};
            normal[axis] = static_cast<float>(-step[axis]);
            if (distance > static_cast<double>(ray.length) || !visit(Cell{cell[0], cell[1], cell[2]}, static_cast<float>(distance), math::Vec3{normal[0], normal[1], normal[2]})) {
                return;
            }
        }
    }

  private:
    static constexpr double kUnreachable = std::numeric_limits<double>::infinity();

    // Cells stay below this magnitude, so stepping past the last cell never overflows.
    static constexpr float kCellLimit = 2147483648.0F;

    static void requireCastable(const math::Ray3D& ray, math::Vec3 cellSize);

    [[nodiscard]] static double boundary(float origin, float direction, float size, int cell) noexcept {
        if (direction > 0.0F) {
            return ((static_cast<double>(cell) + 1.0) * size - origin) / direction;
        }

        if (direction < 0.0F) {
            return (static_cast<double>(cell) * size - origin) / direction;
        }

        return kUnreachable;
    }
};

} // namespace haylen::spatial3d
