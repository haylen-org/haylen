#include "haylen/2d/spatial/FloodFill.hpp"

#include <algorithm>

namespace haylen::spatial2d {

void FloodFill::beginVisit(std::size_t count) {
    if (visits.size() < count) {
        visits.resize(count, 0);
    }
    ++visit;
    if (visit == 0) {
        std::ranges::fill(visits, 0U);
        visit = 1;
    }
}

void FloodFill::fill(const CellGrid& grid, Cell start, bool diagonal, std::vector<Cell>& cells) {
    if (!grid.contains(start)) {
        cells.clear();
        return;
    }
    const std::int32_t value = grid[start];
    fill(start, grid.getWidth(), grid.getHeight(), diagonal, [&grid, value](Cell cell) { return grid[cell] == value; }, cells);
}

} // namespace haylen::spatial2d
