#include "haylen/2d/spatial/ConnectedComponents.hpp"

#include <array>
#include <stdexcept>
#include <vector>

#include "haylen/2d/spatial/UnionFind.hpp"

namespace haylen::spatial2d {

std::size_t ConnectedComponents::label(const CellGrid& grid, const Options& options, CellGrid& labels) {
    if (labels.getWidth() != grid.getWidth() || labels.getHeight() != grid.getHeight()) {
        throw std::invalid_argument("Region labels need a grid of the same size as the labeled grid.");
    }

    // The first pass joins every cell with the neighbors already scanned that hold its value: left and up, plus the upper corners for diagonal regions.
    const std::array<Cell, 4> scanned{{{-1, 0}, {0, -1}, {-1, -1}, {1, -1}}};
    const std::size_t neighbors = options.diagonal ? scanned.size() : 2;
    const auto isBackground = [&options](std::int32_t value) { return options.background && *options.background == value; };
    UnionFind regions(grid.getValues().size());
    for (int y = 0; y < grid.getHeight(); ++y) {
        for (int x = 0; x < grid.getWidth(); ++x) {
            const std::int32_t value = grid[{x, y}];
            if (isBackground(value)) {
                continue;
            }
            for (std::size_t neighbor = 0; neighbor < neighbors; ++neighbor) {
                const Cell other{x + scanned[neighbor].x, y + scanned[neighbor].y};
                if (grid.contains(other) && grid[other] == value) {
                    regions.unite(grid.indexOf({x, y}), grid.indexOf(other));
                }
            }
        }
    }

    // The second pass numbers the regions in the order their first cell appears.
    std::vector<std::int32_t> numbers(grid.getValues().size(), 0);
    std::int32_t count = 0;
    for (int y = 0; y < grid.getHeight(); ++y) {
        for (int x = 0; x < grid.getWidth(); ++x) {
            if (isBackground(grid[{x, y}])) {
                labels.set({x, y}, 0);
                continue;
            }
            std::int32_t& number = numbers[regions.find(grid.indexOf({x, y}))];
            if (number == 0) {
                number = ++count;
            }
            labels.set({x, y}, number);
        }
    }
    return static_cast<std::size_t>(count);
}

} // namespace haylen::spatial2d
