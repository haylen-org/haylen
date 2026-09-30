#include "haylen/2d/spatial/HashGrid.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

#include "2d/spatial/EntryBounds.hpp"
#include "haylen/2d/spatial/GridRay.hpp"
#include "haylen/math/Raycast.hpp"

namespace haylen::spatial2d {

HashGrid::HashGrid(float gridCellSize) : cellSize(gridCellSize) {
    if (!(gridCellSize > 0.0F) || !std::isfinite(gridCellSize)) {
        throw std::invalid_argument("A spatial hash needs a positive cell size.");
    }
}

std::uint64_t HashGrid::cellKey(int x, int y) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32U) | static_cast<std::uint32_t>(y);
}

float HashGrid::cellOf(float value) const noexcept {
    return std::floor(value / cellSize);
}

HashGrid::CellRange HashGrid::entryCellsOf(const math::Rect& bounds) const {
    const float left = cellOf(bounds.getLeft());
    const float top = cellOf(bounds.getTop());
    const float right = cellOf(bounds.getRight());
    const float bottom = cellOf(bounds.getBottom());
    if (!(left >= -kMaxCell && top >= -kMaxCell && right <= kMaxCell && bottom <= kMaxCell)) {
        throw std::invalid_argument("A spatial hash entry must lie within 536870912 cells of the origin.");
    }
    if ((static_cast<double>(right) - left + 1.0) * (static_cast<double>(bottom) - top + 1.0) > kMaxEntryCells) {
        throw std::invalid_argument("A spatial hash entry may cover at most 65536 cells, so the cell size should be closer to the size of the entries.");
    }
    return {.left = static_cast<int>(left), .top = static_cast<int>(top), .right = static_cast<int>(right), .bottom = static_cast<int>(bottom)};
}

HashGrid::CellRange HashGrid::occupiedCellsOf(const math::Rect& area) const noexcept {
    const double left = std::max<double>(cellOf(area.getLeft()), occupied.left);
    const double top = std::max<double>(cellOf(area.getTop()), occupied.top);
    const double right = std::min<double>(cellOf(area.getRight()), occupied.right);
    const double bottom = std::min<double>(cellOf(area.getBottom()), occupied.bottom);
    if (left > right || top > bottom) {
        return {};
    }
    return {.left = static_cast<int>(left), .top = static_cast<int>(top), .right = static_cast<int>(right), .bottom = static_cast<int>(bottom)};
}

void HashGrid::link(std::uint64_t id, const CellRange& cells) {
    for (int y = cells.top; y <= cells.bottom; ++y) {
        for (int x = cells.left; x <= cells.right; ++x) {
            buckets[cellKey(x, y)].push_back(id);
        }
    }

    const bool empty = occupied.right < occupied.left;
    occupied = empty ? cells : CellRange{.left = std::min(occupied.left, cells.left), .top = std::min(occupied.top, cells.top), .right = std::max(occupied.right, cells.right), .bottom = std::max(occupied.bottom, cells.bottom)};
}

void HashGrid::unlink(std::uint64_t id, const CellRange& cells) {
    for (int y = cells.top; y <= cells.bottom; ++y) {
        for (int x = cells.left; x <= cells.right; ++x) {
            const auto bucket = buckets.find(cellKey(x, y));
            std::vector<std::uint64_t>& ids = bucket->second;
            const auto position = std::ranges::find(ids, id);
            *position = ids.back();
            ids.pop_back();
            if (ids.empty()) {
                buckets.erase(bucket);
            }
        }
    }
}

void HashGrid::set(std::uint64_t id, const math::Rect& bounds) {
    EntryBounds::requireValid(bounds);
    const CellRange cells = entryCellsOf(bounds);

    const auto existing = entries.find(id);
    if (existing == entries.end()) {
        entries.emplace(id, Entry{.bounds = bounds, .cells = cells});
        link(id, cells);
        return;
    }

    // Small moves usually stay inside the same cells, which only needs the new bounds.
    Entry& entry = existing->second;
    if (entry.cells != cells) {
        unlink(id, entry.cells);
        link(id, cells);
        entry.cells = cells;
    }
    entry.bounds = bounds;
}

bool HashGrid::remove(std::uint64_t id) {
    const auto entry = entries.find(id);
    if (entry == entries.end()) {
        return false;
    }
    unlink(id, entry->second.cells);
    entries.erase(entry);
    return true;
}

void HashGrid::clear() noexcept {
    buckets.clear();
    entries.clear();
    occupied = {};
}

bool HashGrid::contains(std::uint64_t id) const {
    return entries.contains(id);
}

std::optional<math::Rect> HashGrid::getBounds(std::uint64_t id) const {
    const auto entry = entries.find(id);
    if (entry == entries.end()) {
        return std::nullopt;
    }
    return entry->second.bounds;
}

void HashGrid::collect(const CellRange& cells, std::vector<std::uint64_t>& ids) const {
    ids.clear();

    // A range that covers more cells than there are buckets is cheaper to answer by visiting every bucket.
    const auto columns = static_cast<std::uint64_t>(std::int64_t{cells.right} - cells.left + 1);
    const auto rows = static_cast<std::uint64_t>(std::int64_t{cells.bottom} - cells.top + 1);
    if (columns * rows > buckets.size()) {
        for (const auto& [key, bucket] : buckets) {
            const auto x = static_cast<std::int32_t>(key >> 32U);
            const auto y = static_cast<std::int32_t>(key & 0xFFFFFFFFU);
            if (x >= cells.left && x <= cells.right && y >= cells.top && y <= cells.bottom) {
                ids.insert(ids.end(), bucket.begin(), bucket.end());
            }
        }
    } else {
        for (int y = cells.top; y <= cells.bottom; ++y) {
            for (int x = cells.left; x <= cells.right; ++x) {
                const auto bucket = buckets.find(cellKey(x, y));
                if (bucket != buckets.end()) {
                    ids.insert(ids.end(), bucket->second.begin(), bucket->second.end());
                }
            }
        }
    }
    std::ranges::sort(ids);
    ids.erase(std::ranges::unique(ids).begin(), ids.end());
}

void HashGrid::query(const math::Rect& area, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireValid(area);
    collect(occupiedCellsOf(area), ids);
    std::erase_if(ids, [&](std::uint64_t id) { return !EntryBounds::overlaps(entries.at(id).bounds, area); });
}

void HashGrid::queryCircle(math::Vec2 center, float radius, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireRadius(radius);
    const math::Rect area = math::Rect::fromCenter(center, {radius * 2.0F, radius * 2.0F});
    EntryBounds::requireValid(area);
    collect(occupiedCellsOf(area), ids);
    std::erase_if(ids, [&](std::uint64_t id) { return EntryBounds::distanceSquared(entries.at(id).bounds, center) > radius * radius; });
}

void HashGrid::queryPoint(math::Vec2 point, std::vector<std::uint64_t>& ids) const {
    query({point.x, point.y, 0.0F, 0.0F}, ids);
}

void HashGrid::raycast(const math::Ray& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    hits.clear();
    const math::Rect extent{static_cast<float>(occupied.left) * cellSize, static_cast<float>(occupied.top) * cellSize, static_cast<float>(occupied.right - occupied.left + 1) * cellSize, static_cast<float>(occupied.bottom - occupied.top + 1) * cellSize};
    const std::optional<std::array<float, 2>> travel = entries.empty() ? std::nullopt : math::Raycast::clip(ray, extent);
    if (!travel) {
        return;
    }

    // A ray that crosses more cells than there are entries costs less when it tests every entry.
    const math::Ray inside{ray.at((*travel)[0]), ray.direction, (*travel)[1] - (*travel)[0]};
    const math::Vec2 end = inside.getEnd();
    const double crossed = std::fabs(static_cast<double>(cellOf(end.x)) - cellOf(inside.origin.x)) + std::fabs(static_cast<double>(cellOf(end.y)) - cellOf(inside.origin.y)) + 1.0;
    if (crossed > static_cast<double>(entries.size())) {
        for (const auto& [id, entry] : entries) {
            if (const std::optional<math::RayHit> hit = math::Raycast::rect(ray, entry.bounds)) {
                EntryBounds::insertHit(hits, {.id = id, .point = hit->point, .normal = hit->normal, .distance = hit->distance});
            }
        }
        EntryBounds::finishHits(hits, limit);
        return;
    }

    // Cells are visited in the order the ray crosses them, so once the ray enters a cell beyond the last kept hit, no later entry can come closer.
    // clang-format off
    GridRay::traverse(inside, {cellSize, cellSize}, [&](Cell cell, float distance, math::Vec2) {
        if (limit > 0 && hits.size() >= limit && hits[limit - 1].distance <= (*travel)[0] + distance) {
            return false;
        }
        const auto bucket = buckets.find(cellKey(cell.x, cell.y));
        if (bucket == buckets.end()) {
            return true;
        }
        for (const std::uint64_t id : bucket->second) {
            if (std::ranges::any_of(hits, [id](const RayHit& hit) { return hit.id == id; })) {
                continue;
            }
            if (const std::optional<math::RayHit> hit = math::Raycast::rect(ray, entries.at(id).bounds)) {
                EntryBounds::insertHit(hits, {.id = id, .point = hit->point, .normal = hit->normal, .distance = hit->distance});
            }
        }
        return true;
    });
    // clang-format on
    EntryBounds::finishHits(hits, limit);
}

void HashGrid::offerBucket(Cell cell, Cell center, math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    if (cell.x < occupied.left || cell.x > occupied.right || cell.y < occupied.top || cell.y > occupied.bottom) {
        return;
    }
    const auto bucket = buckets.find(cellKey(cell.x, cell.y));
    if (bucket == buckets.end()) {
        return;
    }

    for (const std::uint64_t id : bucket->second) {
        const Entry& entry = entries.at(id);
        if (cell != Cell{std::clamp(center.x, entry.cells.left, entry.cells.right), std::clamp(center.y, entry.cells.top, entry.cells.bottom)}) {
            continue;
        }
        const float distance = std::sqrt(EntryBounds::distanceSquared(entry.bounds, point));
        if (distance <= maxDistance) {
            EntryBounds::keepNearest(neighbors, {.id = id, .distance = distance}, count);
        }
    }
}

void HashGrid::scanNearest(math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    neighbors.clear();
    for (const auto& [id, entry] : entries) {
        const float distance = std::sqrt(EntryBounds::distanceSquared(entry.bounds, point));
        if (distance <= maxDistance) {
            EntryBounds::keepNearest(neighbors, {.id = id, .distance = distance}, count);
        }
    }
    EntryBounds::finishNearest(neighbors);
}

void HashGrid::nearest(math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    EntryBounds::requirePoint(point);
    EntryBounds::requireRadius(maxDistance);
    neighbors.clear();
    if (count == 0 || entries.empty()) {
        return;
    }

    // Rings of cells grow around the cell of the point. An entry first met in ring `r` misses the inner rings, so it lies at least `r - 1` cells away.
    const double pointX = cellOf(point.x);
    const double pointY = cellOf(point.y);
    const double gap = std::max({0.0, occupied.left - pointX, pointX - occupied.right, occupied.top - pointY, pointY - occupied.bottom});
    if ((gap - 1.0) * cellSize > maxDistance) {
        return;
    }

    // A point beyond the occupied cells searches from the cell next to them, which only lowers the bounds of the rings.
    const Cell center{static_cast<int>(std::clamp(pointX, occupied.left - 1.0, occupied.right + 1.0)), static_cast<int>(std::clamp(pointY, occupied.top - 1.0, occupied.bottom + 1.0))};
    const int firstRing = std::max({0, occupied.left - center.x, center.x - occupied.right, occupied.top - center.y, center.y - occupied.bottom});
    const int lastRing = std::max({std::abs(center.x - occupied.left), std::abs(center.x - occupied.right), std::abs(center.y - occupied.top), std::abs(center.y - occupied.bottom)});
    std::size_t visited = 0;
    for (int ring = firstRing; ring <= lastRing; ++ring) {
        const float nearestPossible = static_cast<float>(ring - 1) * cellSize;
        if (nearestPossible > maxDistance || (neighbors.size() == count && EntryBounds::getFarthest(neighbors).distance < nearestPossible)) {
            break;
        }

        // Rings that visit more cells than there are entries cost more than measuring every entry.
        visited += ring == 0 ? 1 : 8 * static_cast<std::size_t>(ring);
        if (visited > entries.size()) {
            scanNearest(point, count, maxDistance, neighbors);
            return;
        }
        if (ring == 0) {
            offerBucket(center, center, point, count, maxDistance, neighbors);
            continue;
        }
        for (int x = std::max(center.x - ring, occupied.left); x <= std::min(center.x + ring, occupied.right); ++x) {
            offerBucket({x, center.y - ring}, center, point, count, maxDistance, neighbors);
            offerBucket({x, center.y + ring}, center, point, count, maxDistance, neighbors);
        }
        for (int y = std::max(center.y - ring + 1, occupied.top); y <= std::min(center.y + ring - 1, occupied.bottom); ++y) {
            offerBucket({center.x - ring, y}, center, point, count, maxDistance, neighbors);
            offerBucket({center.x + ring, y}, center, point, count, maxDistance, neighbors);
        }
    }
    EntryBounds::finishNearest(neighbors);
}

} // namespace haylen::spatial2d
