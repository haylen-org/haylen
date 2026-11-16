#include "haylen/2d/spatial/Bresenham.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace haylen::spatial2d {

void Bresenham::line(Cell from, Cell to, std::vector<Cell>& cells) {
    const std::int64_t longest = std::max(std::abs(static_cast<std::int64_t>(to.x) - from.x), std::abs(static_cast<std::int64_t>(to.y) - from.y));
    if (longest + 1 > kMaxLineCells) {
        throw std::invalid_argument("A line covers at most " + std::to_string(kMaxLineCells) + " cells.");
    }
    cells.clear();
    const int spanX = std::abs(to.x - from.x);
    const int spanY = -std::abs(to.y - from.y);
    const int stepX = from.x < to.x ? 1 : -1;
    const int stepY = from.y < to.y ? 1 : -1;
    int error = spanX + spanY;

    for (Cell cell = from;;) {
        cells.push_back(cell);

        if (cell == to) {
            return;
        }

        const int doubled = 2 * error;
        if (doubled >= spanY) {
            error += spanY;
            cell.x += stepX;
        }

        if (doubled <= spanX) {
            error += spanX;
            cell.y += stepY;
        }
    }
}

void Bresenham::circle(Cell center, int radius, std::vector<Cell>& cells) {
    if (radius < 0 || radius > kMaxRadius) {
        throw std::invalid_argument("A circle needs a radius from 0 to " + std::to_string(kMaxRadius) + " cells.");
    }

    cells.clear();
    int x = radius;
    int y = 0;
    int error = 1 - radius;
    while (x >= y) {
        for (const Cell offset : {Cell{x, y}, Cell{y, x}, Cell{-y, x}, Cell{-x, y}, Cell{-x, -y}, Cell{-y, -x}, Cell{y, -x}, Cell{x, -y}}) {
            cells.push_back({center.x + offset.x, center.y + offset.y});
        }

        ++y;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            --x;
            error += 2 * (y - x) + 1;
        }
    }

    // The octants meet on the axes and the diagonals, where they produce the same cells twice.
    std::ranges::sort(cells, [](Cell lhs, Cell rhs) { return lhs.y != rhs.y ? lhs.y < rhs.y : lhs.x < rhs.x; });
    cells.erase(std::ranges::unique(cells).begin(), cells.end());
}

} // namespace haylen::spatial2d
