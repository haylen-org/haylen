#include "haylen/2d/navigation/FlowField.hpp"

#include <stdexcept>

namespace haylen::navigation2d {

void FlowField::compute(const Grid& grid, std::span<const Grid::Cell> goals, bool diagonal) {
    sources.clear();
    for (const Grid::Cell goal : goals) {
        sources.push_back({.cell = goal});
    }
    distances.compute(grid, sources, diagonal);

    next.assign(grid.getCellCount(), -1);
    for (std::size_t index = 0; index < next.size(); ++index) {
        if (const std::optional<Grid::Cell> step = distances.getNext(grid, grid.cellAt(index))) {
            next[index] = static_cast<std::int32_t>(grid.indexOf(*step));
        }
    }
}

std::size_t FlowField::indexOf(Grid::Cell cell) const {
    if (cell.x < 0 || cell.y < 0 || cell.x >= getWidth() || cell.y >= getHeight()) {
        throw std::out_of_range("The cell is outside the flow field.");
    }
    return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(getWidth()) + static_cast<std::size_t>(cell.x);
}

std::optional<Grid::Cell> FlowField::getNext(Grid::Cell cell) const {
    const std::int32_t step = next[indexOf(cell)];
    if (step < 0) {
        return std::nullopt;
    }
    return Grid::Cell{step % getWidth(), step / getWidth()};
}

math::Vec2 FlowField::getDirection(Grid::Cell cell) const {
    const std::optional<Grid::Cell> step = getNext(cell);
    if (!step) {
        return {};
    }
    return math::Vec2{static_cast<float>(step->x - cell.x), static_cast<float>(step->y - cell.y)}.getNormalized();
}

float FlowField::getDistance(Grid::Cell cell) const {
    return distances.getValues()[indexOf(cell)];
}

} // namespace haylen::navigation2d
