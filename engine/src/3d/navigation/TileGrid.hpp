#pragma once

#include <cstddef>

#include "haylen/3d/navigation/BuildSettings.hpp"
#include "haylen/math/Box.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::navigation3d {

// The columns of tiles that a navigation mesh covers on the ground, from the smallest corner of its bounds, with the voxels of each tile and the border of voxels that a tile bakes around itself so its edges meet the tiles next to it.
struct TileGrid {
    // The columns from `minX` and `minZ` to `maxX` and `maxZ`, both included.
    struct Range {
        int minX = 0;
        int minZ = 0;
        int maxX = -1;
        int maxZ = -1;
    };

    math::Box bounds;
    float tileWorld = 0.0F;
    int tileCells = 0;
    int borderCells = 0;
    int columns = 0;
    int rows = 0;

    static constexpr std::size_t kMaxColumns = std::size_t{1} << 16U;

    // Throws `std::invalid_argument` for bounds that need more columns than a mesh holds.
    [[nodiscard]] static TileGrid make(const BuildSettings& settings, const math::Box& area);

    [[nodiscard]] std::size_t getColumnCount() const noexcept {
        return static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows);
    }
    [[nodiscard]] std::size_t getIndex(int x, int z) const noexcept {
        return static_cast<std::size_t>(z) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(x);
    }

    // The ground of a column with the height of the bounds of the mesh, without and with its border.
    [[nodiscard]] math::Box getColumnBounds(int x, int z) const noexcept;
    [[nodiscard]] math::Box getBakeBounds(int x, int z, float cellSize) const noexcept;

    // The columns that a box touches on the ground, within the grid.
    [[nodiscard]] Range getRange(const math::Box& area) const noexcept;

  private:
    // The column or row of a coordinate, kept from one before the grid to one past it.
    [[nodiscard]] int getCell(float value, float origin, int count) const noexcept;
};

} // namespace haylen::navigation3d
