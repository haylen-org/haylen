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

int HashGrid::cellOf(float value) const noexcept {
    return static_cast<int>(std::floor(value / cellSize));
}

HashGrid::CellRange HashGrid::cellsOf(const math::Rect& bounds) const noexcept {
    return {.left = cellOf(bounds.getLeft()), .top = cellOf(bounds.getTop()), .right = cellOf(bounds.getRight()), .bottom = cellOf(bounds.getBottom())};
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
    const CellRange cells = cellsOf(bounds);

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
    for (int y = cells.top; y <= cells.bottom; ++y) {
        for (int x = cells.left; x <= cells.right; ++x) {
            const auto bucket = buckets.find(cellKey(x, y));
            if (bucket != buckets.end()) {
                ids.insert(ids.end(), bucket->second.begin(), bucket->second.end());
            }
        }
    }
    std::ranges::sort(ids);
    ids.erase(std::ranges::unique(ids).begin(), ids.end());
}

void HashGrid::query(const math::Rect& area, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireValid(area);
    collect(cellsOf(area), ids);
    std::erase_if(ids, [&](std::uint64_t id) { return !EntryBounds::overlaps(entries.at(id).bounds, area); });
}

void HashGrid::queryCircle(math::Vec2 center, float radius, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireRadius(radius);
    const math::Rect area = math::Rect::fromCenter(center, {radius * 2.0F, radius * 2.0F});
    EntryBounds::requireValid(area);
    collect(cellsOf(area), ids);
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

    // Cells are visited in the order the ray crosses them, so once the ray enters a cell beyond the last kept hit, no later entry can come closer.
    const auto closer = [](const RayHit& lhs, const RayHit& rhs) { return lhs.distance != rhs.distance ? lhs.distance < rhs.distance : lhs.id < rhs.id; };
    const math::Ray inside{ray.at((*travel)[0]), ray.direction, (*travel)[1] - (*travel)[0]};
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
                const RayHit found{.id = id, .point = hit->point, .normal = hit->normal, .distance = hit->distance};
                hits.insert(std::ranges::upper_bound(hits, found, closer), found);
            }
        }
        return true;
    });
    // clang-format on
    EntryBounds::finishHits(hits, limit);
}

void HashGrid::offerBucket(int x, int y, math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    if (x < occupied.left || x > occupied.right || y < occupied.top || y > occupied.bottom) {
        return;
    }
    const auto bucket = buckets.find(cellKey(x, y));
    if (bucket == buckets.end()) {
        return;
    }

    for (const std::uint64_t id : bucket->second) {
        const float distance = std::sqrt(EntryBounds::distanceSquared(entries.at(id).bounds, point));
        if (distance <= maxDistance && std::ranges::none_of(neighbors, [id](const Neighbor& neighbor) { return neighbor.id == id; })) {
            EntryBounds::keepNearest(neighbors, {.id = id, .distance = distance}, count);
        }
    }
}

void HashGrid::nearest(math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    EntryBounds::requireRadius(maxDistance);
    neighbors.clear();
    if (count == 0 || entries.empty()) {
        return;
    }

    // Rings of cells grow around the cell of the point. An entry first met in ring r misses the inner rings, so it lies at least r - 1 cells away.
    const int centerX = cellOf(point.x);
    const int centerY = cellOf(point.y);
    const int firstRing = std::max({0, occupied.left - centerX, centerX - occupied.right, occupied.top - centerY, centerY - occupied.bottom});
    const int lastRing = std::max({std::abs(centerX - occupied.left), std::abs(centerX - occupied.right), std::abs(centerY - occupied.top), std::abs(centerY - occupied.bottom)});
    for (int ring = firstRing; ring <= lastRing; ++ring) {
        const float nearestPossible = static_cast<float>(ring - 1) * cellSize;
        if (nearestPossible > maxDistance || (neighbors.size() == count && neighbors.back().distance <= nearestPossible)) {
            return;
        }
        if (ring == 0) {
            offerBucket(centerX, centerY, point, count, maxDistance, neighbors);
            continue;
        }
        for (int x = std::max(centerX - ring, occupied.left); x <= std::min(centerX + ring, occupied.right); ++x) {
            offerBucket(x, centerY - ring, point, count, maxDistance, neighbors);
            offerBucket(x, centerY + ring, point, count, maxDistance, neighbors);
        }
        for (int y = std::max(centerY - ring + 1, occupied.top); y <= std::min(centerY + ring - 1, occupied.bottom); ++y) {
            offerBucket(centerX - ring, y, point, count, maxDistance, neighbors);
            offerBucket(centerX + ring, y, point, count, maxDistance, neighbors);
        }
    }
}

} // namespace haylen::spatial2d
