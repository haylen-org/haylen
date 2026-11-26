#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "3d/navigation/TileGrid.hpp"
#include "haylen/3d/navigation/SourceGeometry.hpp"

namespace haylen::navigation3d {

// The triangles of a geometry that each column of a grid bakes, its border included, gathered once so every tile reads only its own.
class TileIndex final {
  public:
    TileIndex(const SourceGeometry& source, const TileGrid& tileGrid, float cellSize);

    [[nodiscard]] std::span<const std::int32_t> getTriangles(int x, int z) const noexcept;

    // The triangles of one column alone, for a rebuild of a few tiles.
    static void collect(const SourceGeometry& source, const TileGrid& grid, float cellSize, int x, int z, std::vector<std::int32_t>& triangles);

  private:
    [[nodiscard]] static math::Vec3 getCorner(const SourceGeometry& source, std::size_t triangle, std::size_t corner) noexcept;
    [[nodiscard]] static math::Box getTriangleBounds(const SourceGeometry& source, std::size_t triangle) noexcept;

    const TileGrid* grid = nullptr;
    std::vector<std::size_t> offsets;
    std::vector<std::int32_t> triangles;
};

} // namespace haylen::navigation3d
