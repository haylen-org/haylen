#pragma once

#include <cstdint>
#include <vector>

#include "haylen/2d/spatial/Cell.hpp"

namespace haylen::spatial2d {

// Rasterizes lines and circles on a grid with integer arithmetic only.
class Bresenham final {
  public:
    // The most cells of a line and the largest radius of a circle.
    static constexpr std::int64_t kMaxLineCells = std::int64_t{1} << 20;
    static constexpr int kMaxRadius = 1 << 16;
    // Fills `cells` with the cells of the line from one cell to the other, both included, in order from the first. Lines step diagonally where both axes advance.
    static void line(Cell from, Cell to, std::vector<Cell>& cells);

    // Fills `cells` with the outline of a circle with the midpoint algorithm, each cell once, sorted by row and then by column. A radius of zero gives the center alone.
    static void circle(Cell center, int radius, std::vector<Cell>& cells);
};

} // namespace haylen::spatial2d
