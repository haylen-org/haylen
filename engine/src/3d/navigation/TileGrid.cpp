#include "3d/navigation/TileGrid.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace haylen::navigation3d {

TileGrid TileGrid::make(const BuildSettings& settings, const math::Box& area) {
    TileGrid grid;
    grid.bounds = area;
    grid.tileCells = settings.getTileCells();
    grid.tileWorld = static_cast<float>(grid.tileCells) * settings.cellSize;
    grid.borderCells = static_cast<int>(std::ceil(settings.agentRadius / settings.cellSize)) + 3;

    const math::Vec3 size = area.getSize();
    const double columns = std::max(1.0, std::ceil(static_cast<double>(size.x) / grid.tileWorld));
    const double rows = std::max(1.0, std::ceil(static_cast<double>(size.z) / grid.tileWorld));
    if (columns * rows > static_cast<double>(kMaxColumns)) {
        throw std::invalid_argument("A navigation mesh holds at most 65536 tiles on the ground, so bake a smaller world or a larger \"tileSize\".");
    }

    grid.columns = static_cast<int>(columns);
    grid.rows = static_cast<int>(rows);
    return grid;
}

math::Box TileGrid::getColumnBounds(int x, int z) const noexcept {
    const math::Vec3 min{bounds.min.x + static_cast<float>(x) * tileWorld, bounds.min.y, bounds.min.z + static_cast<float>(z) * tileWorld};
    return {min, math::Vec3{min.x + tileWorld, bounds.max.y, min.z + tileWorld}};
}

math::Box TileGrid::getBakeBounds(int x, int z, float cellSize) const noexcept {
    const float border = static_cast<float>(borderCells) * cellSize;
    const math::Box column = getColumnBounds(x, z);
    return {column.min - math::Vec3{border, 0.0F, border}, column.max + math::Vec3{border, 0.0F, border}};
}

int TileGrid::getCell(float value, float origin, int count) const noexcept {
    const double cell = std::floor((static_cast<double>(value) - static_cast<double>(origin)) / static_cast<double>(tileWorld));
    return static_cast<int>(std::clamp(cell, -1.0, static_cast<double>(count)));
}

TileGrid::Range TileGrid::getRange(const math::Box& area) const noexcept {
    Range range;
    range.minX = std::max(0, getCell(area.min.x, bounds.min.x, columns));
    range.minZ = std::max(0, getCell(area.min.z, bounds.min.z, rows));
    range.maxX = std::min(columns - 1, getCell(area.max.x, bounds.min.x, columns));
    range.maxZ = std::min(rows - 1, getCell(area.max.z, bounds.min.z, rows));
    return range;
}

} // namespace haylen::navigation3d
