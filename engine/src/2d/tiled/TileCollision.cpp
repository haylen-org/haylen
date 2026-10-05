#include "haylen/2d/tiled/TileCollision.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "haylen/math/Geometry.hpp"

namespace haylen::tiled {

TileCollision::TileCollision(physics2d::Body owner, std::uint32_t layerId, math::Vec2 corner, math::Vec2 size, const physics2d::CollisionFilter& shapeFilter) : body(owner), layer(layerId), origin(corner), cellSize(size), filter(shapeFilter) {}

std::int64_t TileCollision::key(int column, int row) noexcept {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(static_cast<std::uint32_t>(row)) << 32U | static_cast<std::uint32_t>(column));
}

int TileCollision::columnOf(std::int64_t cell) noexcept {
    return static_cast<int>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(cell)));
}

int TileCollision::rowOf(std::int64_t cell) noexcept {
    return static_cast<int>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(cell) >> 32U));
}

bool TileCollision::isSolid(const Cell& cell) noexcept {
    return cell.full || !cell.outlines.empty();
}

bool TileCollision::isFull(int column, int row) const noexcept {
    const auto found = cells.find(key(column, row));
    return found != cells.end() && found->second.cell.full;
}

math::Rect TileCollision::cellBounds(int column, int row) const noexcept {
    return {origin.x + static_cast<float>(column) * cellSize.x, origin.y + static_cast<float>(row) * cellSize.y, cellSize.x, cellSize.y};
}

int TileCollision::getReach() const noexcept {
    return 1 + extent * 2;
}

void TileCollision::setCell(int column, int row, Cell value) {
    const std::int64_t cell = key(column, row);
    if (const auto found = cells.find(cell); found != cells.end()) {
        for (physics2d::Shape& shape : found->second.cell.shapes) {
            shape.destroy();
        }
        cells.erase(found);
    }
    changed.push_back(cell);
    if (!isSolid(value) && value.shapes.empty()) {
        return;
    }

    // Every outline winds the same way, so overlapping shapes add up instead of cutting holes into each other.
    for (math::Polygon::Outline& outline : value.outlines) {
        if (math::Geometry::signedArea(outline) < 0.0F) {
            std::reverse(outline.begin(), outline.end());
        }
    }

    // Shapes that stand out of their cell widen the neighborhood where regions look for touching shapes.
    const math::Rect own = cellBounds(column, row);
    math::Rect bounds = value.full ? own : math::Geometry::bounds(value.outlines.empty() ? std::span<const math::Vec2>{} : value.outlines.front());
    for (const math::Polygon::Outline& outline : value.outlines) {
        bounds = bounds.merged(math::Geometry::bounds(outline));
    }
    if (isSolid(value)) {
        const float over = std::max({(own.getLeft() - bounds.getLeft()) / cellSize.x, (bounds.getRight() - own.getRight()) / cellSize.x, (own.getTop() - bounds.getTop()) / cellSize.y, (bounds.getBottom() - own.getBottom()) / cellSize.y});
        extent = std::max(extent, static_cast<int>(std::ceil(over - kTouch)));
    }
    cells.emplace(cell, Stored{.cell = std::move(value), .bounds = bounds});
}

void TileCollision::update() {
    if (changed.empty()) {
        return;
    }

    // The regions near a changed cell may split or join, so they are traced again from all of their cells.
    std::vector<std::int64_t> seeds;
    const int reach = getReach();
    for (const std::int64_t cell : changed) {
        seeds.push_back(cell);
        for (int row = rowOf(cell) - reach; row <= rowOf(cell) + reach; ++row) {
            for (int column = columnOf(cell) - reach; column <= columnOf(cell) + reach; ++column) {
                const auto found = regionOf.find(key(column, row));
                if (found == regionOf.end()) {
                    continue;
                }
                const std::size_t index = found->second;
                seeds.insert(seeds.end(), regions[index].cells.begin(), regions[index].cells.end());
                release(index);
            }
        }
    }
    changed.clear();

    for (const std::int64_t seed : seeds) {
        gather(seed);
    }
}

void TileCollision::release(std::size_t index) {
    Region& region = regions[index];
    for (physics2d::Shape& shape : region.shapes) {
        shape.destroy();
    }
    for (const std::int64_t cell : region.cells) {
        regionOf.erase(cell);
    }
    region.shapes.clear();
    region.cells.clear();
    freeRegions.push_back(index);
}

void TileCollision::gather(std::int64_t seed) {
    const auto start = cells.find(seed);
    if (start == cells.end() || !isSolid(start->second.cell) || regionOf.contains(seed)) {
        return;
    }
    std::size_t index = regions.size();
    if (freeRegions.empty()) {
        regions.emplace_back();
    } else {
        index = freeRegions.back();
        freeRegions.pop_back();
    }

    // The region grows over every solid cell nearby whose shapes touch the shapes of a cell already in it.
    Region& region = regions[index];
    region.cells.push_back(seed);
    regionOf.emplace(seed, index);
    const int reach = getReach();
    for (std::size_t next = 0; next < region.cells.size(); ++next) {
        const std::int64_t cell = region.cells[next];
        const math::Rect near = cells.at(cell).bounds.expanded(kTouch);
        for (int row = rowOf(cell) - reach; row <= rowOf(cell) + reach; ++row) {
            for (int column = columnOf(cell) - reach; column <= columnOf(cell) + reach; ++column) {
                const std::int64_t neighbor = key(column, row);
                const auto found = cells.find(neighbor);
                if (found == cells.end() || !isSolid(found->second.cell) || regionOf.contains(neighbor) || !near.intersects(found->second.bounds)) {
                    continue;
                }
                regionOf.emplace(neighbor, index);
                region.cells.push_back(neighbor);
            }
        }
    }
    build(region);
}

std::vector<math::Polygon::Outline> TileCollision::traceFullCells(std::span<const std::int64_t> full) const {
    std::vector<Edge> edges;
    std::unordered_map<std::int64_t, std::array<int, 2>> starts;
    for (const std::int64_t cell : full) {
        const int column = columnOf(cell);
        const int row = rowOf(cell);
        const std::array<std::int64_t, 4> corners{key(column, row), key(column + 1, row), key(column + 1, row + 1), key(column, row + 1)};
        for (int direction = 0; direction < 4; ++direction) {
            const int outsideColumn = column + kSteps[static_cast<std::size_t>((direction + 3) % 4)][0];
            const int outsideRow = row + kSteps[static_cast<std::size_t>((direction + 3) % 4)][1];
            if (isFull(outsideColumn, outsideRow)) {
                continue;
            }
            const std::int64_t first = corners[static_cast<std::size_t>(direction)];
            auto& slots = starts.try_emplace(first, std::array<int, 2>{-1, -1}).first->second;
            slots[slots[0] < 0 ? 0 : 1] = static_cast<int>(edges.size());
            edges.push_back({.start = first, .end = corners[static_cast<std::size_t>((direction + 1) % 4)], .direction = direction});
        }
    }

    std::vector<math::Polygon::Outline> outlines;
    for (std::size_t first = 0; first < edges.size(); ++first) {
        if (edges[first].traced) {
            continue;
        }
        math::Polygon::Outline outline;
        int incoming = edges[first].direction;
        std::size_t current = first;
        do {
            Edge& edge = edges[current];
            edge.traced = true;
            if (edge.direction != incoming || current == first) {
                outline.push_back({origin.x + static_cast<float>(columnOf(edge.start)) * cellSize.x, origin.y + static_cast<float>(rowOf(edge.start)) * cellSize.y});
            }
            incoming = edge.direction;

            // Where two cells touch only at a corner, the right turn keeps each outline around its own cells, so outlines never cross themselves.
            const std::array<int, 2>& next = starts.at(edge.end);
            current = static_cast<std::size_t>(next[1] >= 0 && edges[static_cast<std::size_t>(next[0])].direction != (edge.direction + 1) % 4 ? next[1] : next[0]);
        } while (current != first);

        // The first corner lies on a straight side when the outline ends with the direction it started with.
        if (incoming == edges[first].direction) {
            outline.erase(outline.begin());
        }
        outlines.push_back(std::move(outline));
    }
    return outlines;
}

void TileCollision::build(Region& region) {
    std::vector<std::int64_t> full;
    std::vector<math::Polygon::Outline> partial;
    for (const std::int64_t cell : region.cells) {
        const Cell& stored = cells.at(cell).cell;
        if (stored.full) {
            full.push_back(cell);
        }
        partial.insert(partial.end(), stored.outlines.begin(), stored.outlines.end());
    }
    std::vector<math::Polygon::Outline> outlines = traceFullCells(full);
    if (!partial.empty()) {
        outlines = math::Polygon::unite(outlines, partial);
    }

    // A chain loop needs four corners, so a lone triangle stays a polygon.
    for (const math::Polygon::Outline& outline : outlines) {
        const math::Polygon::Outline points = math::Polygon::simplify(outline, kStraightness);
        if (points.size() < 3) {
            continue;
        }
        if (points.size() == 3) {
            const std::vector<physics2d::Shape> shapes = body.addPolygon(points, {.filter = filter});
            region.shapes.insert(region.shapes.end(), shapes.begin(), shapes.end());
            continue;
        }
        region.shapes.push_back(body.addChain(points, true, {.filter = filter}).front());
    }
}

} // namespace haylen::tiled
