#include "haylen/2d/spatial/CellGrid.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace haylen::spatial2d {

CellGrid::CellGrid(int columns, int rows, std::int32_t value) : width(columns), height(rows) {
    if (columns <= 0 || rows <= 0 || static_cast<std::int64_t>(columns) * rows > std::numeric_limits<std::int32_t>::max()) {
        throw std::invalid_argument("A cell grid needs a positive size that fits in 32-bit cell indices.");
    }
    values.assign(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows), value);
}

void CellGrid::requireInside(Cell cell) const {
    if (!contains(cell)) {
        throw std::out_of_range("Cell " + std::to_string(cell.x) + "," + std::to_string(cell.y) + " is outside the cell grid.");
    }
}

std::int32_t CellGrid::get(Cell cell) const {
    requireInside(cell);
    return values[indexOf(cell)];
}

void CellGrid::set(Cell cell, std::int32_t value) {
    requireInside(cell);
    values[indexOf(cell)] = value;
}

void CellGrid::fill(std::int32_t value) noexcept {
    std::ranges::fill(values, value);
}

} // namespace haylen::spatial2d
