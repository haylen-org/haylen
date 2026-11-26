#pragma once

namespace haylen::spatial3d {

// A cell of a grid of boxes, addressed by its column, row and layer along `x`, `y` and `z`, which counts from the cell at the world origin.
struct Cell {
    int x = 0;
    int y = 0;
    int z = 0;

    [[nodiscard]] bool operator==(const Cell&) const noexcept = default;
};

} // namespace haylen::spatial3d
