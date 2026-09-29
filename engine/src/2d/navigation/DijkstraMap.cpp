#include "haylen/2d/navigation/DijkstraMap.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace haylen::navigation2d {

bool DijkstraMap::isWorse(const OpenNode& lhs, const OpenNode& rhs) noexcept {
    return lhs.value != rhs.value ? lhs.value > rhs.value : lhs.cell > rhs.cell;
}

void DijkstraMap::requireGrid(const Grid& grid) const {
    if (grid.getWidth() != width || grid.getHeight() != height) {
        throw std::invalid_argument("A Dijkstra map only works with a grid of the size it was computed on.");
    }
}

void DijkstraMap::compute(const Grid& grid, std::span<const Source> sources, bool diagonalSteps) {
    width = grid.getWidth();
    height = grid.getHeight();
    diagonal = diagonalSteps;
    values.assign(grid.getCellCount(), std::numeric_limits<float>::infinity());
    for (const Source& source : sources) {
        if (!std::isfinite(source.value)) {
            throw std::invalid_argument("A Dijkstra map source needs a finite value.");
        }
        if (grid.isWalkable(source.cell)) {
            float& value = values[grid.indexOf(source.cell)];
            value = std::min(value, source.value);
        }
    }
    walk(grid);
}

void DijkstraMap::flee(const Grid& grid, float coefficient) {
    requireGrid(grid);
    if (!(coefficient < 0.0F) || !std::isfinite(coefficient)) {
        throw std::invalid_argument("A flee map needs a finite negative coefficient.");
    }
    for (float& value : values) {
        if (std::isfinite(value)) {
            value *= coefficient;
        }
    }
    walk(grid);
}

void DijkstraMap::walk(const Grid& grid) {
    open.clear();
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (std::isfinite(values[index])) {
            open.push_back({.value = values[index], .cell = static_cast<std::int32_t>(index)});
        }
    }
    std::ranges::make_heap(open, &DijkstraMap::isWorse);

    // Values only fall, so a popped node whose value changed since it was pushed is stale and skipped.
    while (!open.empty()) {
        std::ranges::pop_heap(open, &DijkstraMap::isWorse);
        const OpenNode current = open.back();
        open.pop_back();
        const auto index = static_cast<std::size_t>(current.cell);
        if (current.value != values[index]) {
            continue;
        }

        // Stepping from a neighbor into this cell costs the step length times the cost of this cell.
        const float entry = grid.getCostAt(index);
        // clang-format off
        grid.forEachStep(grid.cellAt(index), diagonal, [&](Grid::Cell neighbor, float length) {
            const std::size_t target = grid.indexOf(neighbor);
            const float value = current.value + length * entry;
            if (value < values[target]) {
                values[target] = value;
                open.push_back({.value = value, .cell = static_cast<std::int32_t>(target)});
                std::ranges::push_heap(open, &DijkstraMap::isWorse);
            }
        });
        // clang-format on
    }
}

float DijkstraMap::getValue(Grid::Cell cell) const {
    if (cell.x < 0 || cell.y < 0 || cell.x >= width || cell.y >= height) {
        throw std::out_of_range("The cell is outside the Dijkstra map.");
    }
    return values[static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(cell.x)];
}

std::optional<Grid::Cell> DijkstraMap::getNext(const Grid& grid, Grid::Cell cell) const {
    requireGrid(grid);
    if (!grid.isWalkable(cell) || !std::isfinite(values[grid.indexOf(cell)])) {
        return std::nullopt;
    }

    const float here = values[grid.indexOf(cell)];
    std::optional<Grid::Cell> best;
    float bestTotal = std::numeric_limits<float>::infinity();
    // clang-format off
    grid.forEachStep(cell, diagonal, [&](Grid::Cell neighbor, float length) {
        const std::size_t index = grid.indexOf(neighbor);
        const float total = values[index] + length * grid.getCostAt(index);
        if (values[index] < here && total < bestTotal) {
            bestTotal = total;
            best = neighbor;
        }
    });
    // clang-format on
    return best;
}

} // namespace haylen::navigation2d
