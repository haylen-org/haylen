#pragma once

namespace haylen::spatial2d {

// A grid cell addressed by column and row, counting from zero like Tiled cells.
struct Cell {
    int x = 0;
    int y = 0;

    [[nodiscard]] bool operator==(const Cell&) const noexcept = default;
};

} // namespace haylen::spatial2d
