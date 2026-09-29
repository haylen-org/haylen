#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "haylen/2d/spatial/Cell.hpp"
#include "haylen/2d/spatial/CellGrid.hpp"

namespace haylen::spatial2d {

// Finds the cells connected to a start cell, like the paint bucket of an image editor. It keeps its visit marks between calls, so repeated fills allocate nothing once its buffers have grown.
class FloodFill final {
  public:
    // Fills cells with the cells of a width by height grid that connect to the start through cells for which inside(cell) is true, in breadth-first order from the start. Diagonal fills also step between cells that only share a corner. A start outside the grid or not inside gives no cells.
    template <typename Inside> void fill(Cell start, int width, int height, bool diagonal, Inside&& inside, std::vector<Cell>& cells) {
        cells.clear();
        if (start.x < 0 || start.y < 0 || start.x >= width || start.y >= height || !inside(start)) {
            return;
        }

        beginVisit(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
        const auto indexOf = [width](Cell cell) { return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(cell.x); };
        visits[indexOf(start)] = visit;
        cells.push_back(start);

        const std::size_t directions = diagonal ? kNeighbors.size() : 4;
        for (std::size_t next = 0; next < cells.size(); ++next) {
            const Cell cell = cells[next];
            for (std::size_t direction = 0; direction < directions; ++direction) {
                const Cell neighbor{cell.x + kNeighbors[direction].x, cell.y + kNeighbors[direction].y};
                if (neighbor.x < 0 || neighbor.y < 0 || neighbor.x >= width || neighbor.y >= height || visits[indexOf(neighbor)] == visit || !inside(neighbor)) {
                    continue;
                }
                visits[indexOf(neighbor)] = visit;
                cells.push_back(neighbor);
            }
        }
    }

    // Fills cells with the region of cells that hold the value of the start cell and connect to it.
    void fill(const CellGrid& grid, Cell start, bool diagonal, std::vector<Cell>& cells);

  private:
    // The four sides come first, so side fills read only the first four.
    static constexpr std::array<Cell, 8> kNeighbors{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}}};

    // Starts a fill over the given number of cells with a fresh mark, clearing old marks only when the mark counter wraps around.
    void beginVisit(std::size_t count);

    std::vector<std::uint32_t> visits;
    std::uint32_t visit = 0;
};

} // namespace haylen::spatial2d
