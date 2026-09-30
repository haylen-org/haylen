#pragma once

#include <cmath>
#include <concepts>
#include <limits>
#include <optional>

#include "haylen/2d/spatial/Cell.hpp"
#include "haylen/2d/spatial/CellGrid.hpp"
#include "haylen/math/Ray.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::spatial2d {

// Walks the cells that a ray crosses on a grid of equal cells whose first cell starts at the world origin, with the digital differential analyzer of Amanatides and Woo. A ray through the exact corner of four cells steps along x first.
class GridRay final {
  public:
    struct Hit {
        Cell cell;
        math::Vec2 point{};
        math::Vec2 normal{};
        float distance = 0.0F;
    };

    // Calls `visit(cell, distance, normal)` for every crossed cell in order, with the distance where the ray enters the cell and the normal of the side it enters through, which is zero for the cell of the origin. Visiting stops when `visit` returns `false` or the ray ends, so the ray needs a finite length within the 32-bit range of cells and the cells a positive size, or it throws `std::invalid_argument`.
    template <typename Visit> static void traverse(const math::Ray& ray, math::Vec2 cellSize, Visit&& visit) {
        requireCastable(ray, cellSize);
        Cell cell{static_cast<int>(std::floor(ray.origin.x / cellSize.x)), static_cast<int>(std::floor(ray.origin.y / cellSize.y))};
        const int stepX = ray.direction.x > 0.0F ? 1 : -1;
        const int stepY = ray.direction.y > 0.0F ? 1 : -1;

        // The distances to the next boundaries add up in double precision, which keeps them growing across every cell of the 32-bit range where a float stops changing after about 2^24 steps.
        double nextX = boundary(ray.origin.x, ray.direction.x, cellSize.x, cell.x);
        double nextY = boundary(ray.origin.y, ray.direction.y, cellSize.y, cell.y);
        const double deltaX = ray.direction.x != 0.0F ? static_cast<double>(cellSize.x) / std::fabs(static_cast<double>(ray.direction.x)) : kUnreachable;
        const double deltaY = ray.direction.y != 0.0F ? static_cast<double>(cellSize.y) / std::fabs(static_cast<double>(ray.direction.y)) : kUnreachable;
        if (!visit(cell, 0.0F, math::Vec2{})) {
            return;
        }

        for (;;) {
            double distance = 0.0;
            math::Vec2 normal;
            if (nextX <= nextY) {
                distance = nextX;
                cell.x += stepX;
                nextX += deltaX;
                normal.x = static_cast<float>(-stepX);
            } else {
                distance = nextY;
                cell.y += stepY;
                nextY += deltaY;
                normal.y = static_cast<float>(-stepY);
            }
            if (distance > static_cast<double>(ray.length) || !visit(cell, static_cast<float>(distance), normal)) {
                return;
            }
        }
    }

    // Returns the first crossed cell for which `blocked(cell)` is `true`. A ray that starts in a blocked cell hits it at distance zero with a zero normal.
    template <typename Blocked>
        requires std::predicate<Blocked&, Cell>
    [[nodiscard]] static std::optional<Hit> cast(const math::Ray& ray, math::Vec2 cellSize, Blocked&& blocked) {
        std::optional<Hit> hit;
        // clang-format off
        traverse(ray, cellSize, [&](Cell cell, float distance, math::Vec2 normal) {
            if (!blocked(cell)) {
                return true;
            }
            hit = Hit{cell, ray.at(distance), normal, distance};
            return false;
        });
        // clang-format on
        return hit;
    }

    // Casts against the solid cells of a grid. The ray only travels while it is over the grid, so its length may be infinite, and it enters a grid from outside through the side of the grid. Throws `std::invalid_argument` for cells without a positive and finite size, even when the ray misses the grid.
    [[nodiscard]] static std::optional<Hit> cast(const math::Ray& ray, math::Vec2 cellSize, const CellGrid& grid);

  private:
    static constexpr double kUnreachable = std::numeric_limits<double>::infinity();

    // Cells stay below this magnitude, so stepping past the last cell never overflows.
    static constexpr float kCellLimit = 2147483648.0F;

    static void requireCellSize(math::Vec2 cellSize);
    static void requireCastable(const math::Ray& ray, math::Vec2 cellSize);

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

} // namespace haylen::spatial2d
