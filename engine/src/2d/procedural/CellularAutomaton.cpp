#include "haylen/2d/procedural/CellularAutomaton.hpp"

#include "haylen/math/Random.hpp"

namespace haylen::procedural2d {

bool CellularAutomaton::isBorder(spatial2d::Cell cell, int width, int height) noexcept {
    return cell.x == 0 || cell.y == 0 || cell.x == width - 1 || cell.y == height - 1;
}

int CellularAutomaton::countWalls(const spatial2d::CellGrid& grid, spatial2d::Cell cell, bool solidBorder) noexcept {
    int walls = 0;
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            const spatial2d::Cell neighbor{cell.x + offsetX, cell.y + offsetY};
            if (neighbor == cell) {
                continue;
            }
            if (grid.contains(neighbor) ? grid[neighbor] == kWall : solidBorder) {
                ++walls;
            }
        }
    }
    return walls;
}

spatial2d::CellGrid CellularAutomaton::step(const spatial2d::CellGrid& grid, const Options& options) {
    spatial2d::CellGrid next(grid.getWidth(), grid.getHeight(), kFloor);
    for (int y = 0; y < grid.getHeight(); ++y) {
        for (int x = 0; x < grid.getWidth(); ++x) {
            const spatial2d::Cell cell{x, y};
            const int walls = countWalls(grid, cell, options.solidBorder);
            const bool wall = grid[cell] == kWall ? walls >= options.survivalLimit : walls >= options.birthLimit;
            next.set(cell, (options.solidBorder && isBorder(cell, grid.getWidth(), grid.getHeight())) || wall ? kWall : kFloor);
        }
    }
    return next;
}

spatial2d::CellGrid CellularAutomaton::generate(const Options& options, math::Random& random) {
    spatial2d::CellGrid grid(options.width, options.height, kFloor);
    for (int y = 0; y < options.height; ++y) {
        for (int x = 0; x < options.width; ++x) {
            const spatial2d::Cell cell{x, y};
            grid.set(cell, (options.solidBorder && isBorder(cell, options.width, options.height)) || random.chance(options.fillChance) ? kWall : kFloor);
        }
    }

    for (int iteration = 0; iteration < options.steps; ++iteration) {
        grid = step(grid, options);
    }
    return grid;
}

} // namespace haylen::procedural2d
