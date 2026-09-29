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

    // Calls visit(cell, distance, normal) for every crossed cell in order, with the distance where the ray enters the cell and the normal of the side it enters through, which is zero for the cell of the origin. Visiting stops when visit returns false or the ray ends, so the ray needs a finite length and the cells a positive size.
    template <typename Visit> static void traverse(const math::Ray& ray, math::Vec2 cellSize, Visit&& visit) {
        requireCastable(ray, cellSize);
        Cell cell{static_cast<int>(std::floor(ray.origin.x / cellSize.x)), static_cast<int>(std::floor(ray.origin.y / cellSize.y))};
        const int stepX = ray.direction.x > 0.0F ? 1 : -1;
        const int stepY = ray.direction.y > 0.0F ? 1 : -1;
        float nextX = boundary(ray.origin.x, ray.direction.x, cellSize.x, cell.x);
        float nextY = boundary(ray.origin.y, ray.direction.y, cellSize.y, cell.y);
        const float deltaX = ray.direction.x != 0.0F ? cellSize.x / std::fabs(ray.direction.x) : kUnreachable;
        const float deltaY = ray.direction.y != 0.0F ? cellSize.y / std::fabs(ray.direction.y) : kUnreachable;
        if (!visit(cell, 0.0F, math::Vec2{})) {
            return;
        }

        for (;;) {
            float distance = 0.0F;
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
            if (distance > ray.length || !visit(cell, distance, normal)) {
                return;
            }
        }
    }

    // Returns the first crossed cell for which blocked(cell) is true. A ray that starts in a blocked cell hits it at distance zero with a zero normal.
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

    // Casts against the solid cells of a grid. The ray only travels while it is over the grid, so its length may be infinite, and it enters a grid from outside through the side of the grid.
    [[nodiscard]] static std::optional<Hit> cast(const math::Ray& ray, math::Vec2 cellSize, const CellGrid& grid);

  private:
    static constexpr float kUnreachable = std::numeric_limits<float>::infinity();

    static void requireCastable(const math::Ray& ray, math::Vec2 cellSize);

    [[nodiscard]] static float boundary(float origin, float direction, float size, int cell) noexcept {
        if (direction > 0.0F) {
            return (static_cast<float>(cell + 1) * size - origin) / direction;
        }
        if (direction < 0.0F) {
            return (static_cast<float>(cell) * size - origin) / direction;
        }
        return kUnreachable;
    }
};

} // namespace haylen::spatial2d
