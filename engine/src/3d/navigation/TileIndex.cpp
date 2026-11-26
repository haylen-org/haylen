#include "3d/navigation/TileIndex.hpp"

namespace haylen::navigation3d {

math::Vec3 TileIndex::getCorner(const SourceGeometry& source, std::size_t triangle, std::size_t corner) noexcept {
    const auto vertex = static_cast<std::size_t>(source.getTriangles()[triangle * 3 + corner]) * 3;
    const std::vector<float>& vertices = source.getVertices();
    return {vertices[vertex], vertices[vertex + 1], vertices[vertex + 2]};
}

math::Box TileIndex::getTriangleBounds(const SourceGeometry& source, std::size_t triangle) noexcept {
    const math::Vec3 first = getCorner(source, triangle, 0);
    return math::Box{first, first}.merged(getCorner(source, triangle, 1)).merged(getCorner(source, triangle, 2));
}

TileIndex::TileIndex(const SourceGeometry& source, const TileGrid& tileGrid, float cellSize) : grid(&tileGrid) {
    const float border = static_cast<float>(tileGrid.borderCells) * cellSize;
    const math::Vec3 reach{border, 0.0F, border};
    const std::size_t triangleCount = source.getTriangleCount();

    // The first pass counts the triangles of each column and the second one places them.
    offsets.assign(tileGrid.getColumnCount() + 1, 0);
    std::vector<TileGrid::Range> ranges(triangleCount);
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        const math::Box bounds = getTriangleBounds(source, triangle);
        ranges[triangle] = tileGrid.getRange({bounds.min - reach, bounds.max + reach});
        for (int z = ranges[triangle].minZ; z <= ranges[triangle].maxZ; ++z) {
            for (int x = ranges[triangle].minX; x <= ranges[triangle].maxX; ++x) {
                ++offsets[tileGrid.getIndex(x, z) + 1];
            }
        }
    }

    for (std::size_t column = 1; column < offsets.size(); ++column) {
        offsets[column] += offsets[column - 1];
    }

    triangles.resize(offsets.back());
    std::vector<std::size_t> cursors(offsets.begin(), offsets.end() - 1);
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        for (int z = ranges[triangle].minZ; z <= ranges[triangle].maxZ; ++z) {
            for (int x = ranges[triangle].minX; x <= ranges[triangle].maxX; ++x) {
                triangles[cursors[tileGrid.getIndex(x, z)]++] = static_cast<std::int32_t>(triangle);
            }
        }
    }
}

std::span<const std::int32_t> TileIndex::getTriangles(int x, int z) const noexcept {
    const std::size_t column = grid->getIndex(x, z);
    return std::span<const std::int32_t>(triangles).subspan(offsets[column], offsets[column + 1] - offsets[column]);
}

void TileIndex::collect(const SourceGeometry& source, const TileGrid& grid, float cellSize, int x, int z, std::vector<std::int32_t>& triangles) {
    const math::Box area = grid.getBakeBounds(x, z, cellSize);
    triangles.clear();
    for (std::size_t triangle = 0; triangle < source.getTriangleCount(); ++triangle) {
        const math::Box bounds = getTriangleBounds(source, triangle);
        if (bounds.min.x <= area.max.x && bounds.max.x >= area.min.x && bounds.min.z <= area.max.z && bounds.max.z >= area.min.z) {
            triangles.push_back(static_cast<std::int32_t>(triangle));
        }
    }
}

} // namespace haylen::navigation3d
