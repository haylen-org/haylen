#include "haylen/3d/spatial/HashGrid.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

#include "3d/spatial/EntryBounds.hpp"
#include "haylen/3d/spatial/GridRay.hpp"

namespace haylen::spatial3d {

HashGrid::HashGrid(float gridCellSize) : cellSize(gridCellSize) {
    if (!(gridCellSize > 0.0F) || !std::isfinite(gridCellSize)) {
        throw std::invalid_argument("A spatial hash needs a positive cell size.");
    }
}

// Each coordinate of a cell takes 21 bits, shifted to stay positive.
std::uint64_t HashGrid::cellKey(Cell cell) noexcept {
    const auto pack = [](int value) { return static_cast<std::uint64_t>(static_cast<std::uint32_t>(value) + kCellOffset) & 0x1FFFFFU; };
    return (pack(cell.x) << 42U) | (pack(cell.y) << 21U) | pack(cell.z);
}

Cell HashGrid::cellOfKey(std::uint64_t key) noexcept {
    const auto unpack = [](std::uint64_t bits) { return static_cast<int>(static_cast<std::int64_t>(bits & 0x1FFFFFU) - kCellOffset); };
    return {unpack(key >> 42U), unpack(key >> 21U), unpack(key)};
}

float HashGrid::cellOf(float value) const noexcept {
    return std::floor(value / cellSize);
}

std::array<float, 6> HashGrid::getCellLimits(const math::Box& bounds) const noexcept {
    return {cellOf(bounds.min.x), cellOf(bounds.min.y), cellOf(bounds.min.z), cellOf(bounds.max.x), cellOf(bounds.max.y), cellOf(bounds.max.z)};
}

void HashGrid::requireCells(const math::Box& bounds) const {
    EntryBounds::requireValid(bounds);
    const std::array<float, 6> cells = getCellLimits(bounds);
    if (!std::ranges::all_of(cells, [](float cell) { return std::fabs(cell) <= kMaxCell; })) {
        throw std::invalid_argument("A spatial hash entry must lie within 1048575 cells of the origin.");
    }

    const double covered = (static_cast<double>(cells[3]) - cells[0] + 1.0) * (static_cast<double>(cells[4]) - cells[1] + 1.0) * (static_cast<double>(cells[5]) - cells[2] + 1.0);
    if (covered > static_cast<double>(kMaxEntryCells)) {
        throw std::invalid_argument("A spatial hash entry may cover at most 65536 cells, so the cell size should be closer to the size of the entries.");
    }
}

HashGrid::CellRange HashGrid::entryCellsOf(const math::Box& bounds) const {
    requireCells(bounds);
    const std::array<float, 6> cells = getCellLimits(bounds);
    return {.low = {static_cast<int>(cells[0]), static_cast<int>(cells[1]), static_cast<int>(cells[2])}, .high = {static_cast<int>(cells[3]), static_cast<int>(cells[4]), static_cast<int>(cells[5])}};
}

HashGrid::CellRange HashGrid::occupiedCellsOf(const math::Box& area) const noexcept {
    if (occupied.isEmpty()) {
        return {};
    }

    const auto clamped = [this](float value, int low, int high) { return std::clamp<double>(cellOf(value), low, high); };
    const double lowX = clamped(area.min.x, occupied.low.x, occupied.high.x + 1);
    const double lowY = clamped(area.min.y, occupied.low.y, occupied.high.y + 1);
    const double lowZ = clamped(area.min.z, occupied.low.z, occupied.high.z + 1);
    const double highX = clamped(area.max.x, occupied.low.x - 1, occupied.high.x);
    const double highY = clamped(area.max.y, occupied.low.y - 1, occupied.high.y);
    const double highZ = clamped(area.max.z, occupied.low.z - 1, occupied.high.z);
    if (lowX > highX || lowY > highY || lowZ > highZ) {
        return {};
    }

    return {.low = {static_cast<int>(lowX), static_cast<int>(lowY), static_cast<int>(lowZ)}, .high = {static_cast<int>(highX), static_cast<int>(highY), static_cast<int>(highZ)}};
}

math::Box HashGrid::getOccupiedBox() const noexcept {
    const math::Vec3 low{static_cast<float>(occupied.low.x), static_cast<float>(occupied.low.y), static_cast<float>(occupied.low.z)};
    const math::Vec3 high{static_cast<float>(occupied.high.x + 1), static_cast<float>(occupied.high.y + 1), static_cast<float>(occupied.high.z + 1)};
    return {low * cellSize, high * cellSize};
}

void HashGrid::link(std::uint64_t id, const CellRange& cells) {
    for (int z = cells.low.z; z <= cells.high.z; ++z) {
        for (int y = cells.low.y; y <= cells.high.y; ++y) {
            for (int x = cells.low.x; x <= cells.high.x; ++x) {
                buckets[cellKey({x, y, z})].push_back(id);
            }
        }
    }

    if (occupied.isEmpty()) {
        occupied = cells;
        return;
    }

    occupied.low = {std::min(occupied.low.x, cells.low.x), std::min(occupied.low.y, cells.low.y), std::min(occupied.low.z, cells.low.z)};
    occupied.high = {std::max(occupied.high.x, cells.high.x), std::max(occupied.high.y, cells.high.y), std::max(occupied.high.z, cells.high.z)};
}

void HashGrid::unlink(std::uint64_t id, const CellRange& cells) {
    for (int z = cells.low.z; z <= cells.high.z; ++z) {
        for (int y = cells.low.y; y <= cells.high.y; ++y) {
            for (int x = cells.low.x; x <= cells.high.x; ++x) {
                const auto bucket = buckets.find(cellKey({x, y, z}));
                std::vector<std::uint64_t>& ids = bucket->second;
                *std::ranges::find(ids, id) = ids.back();
                ids.pop_back();
                if (ids.empty()) {
                    buckets.erase(bucket);
                }
            }
        }
    }
}

void HashGrid::set(std::uint64_t id, const math::Box& bounds) {
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

void HashGrid::setAll(std::span<const std::uint64_t> ids, std::span<const math::Box> bounds) {
    EntryBounds::requireBatch(ids.size(), bounds.size());
    for (const math::Box& box : bounds) {
        requireCells(box);
    }

    for (std::size_t index = 0; index < ids.size(); ++index) {
        set(ids[index], bounds[index]);
    }
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

std::optional<math::Box> HashGrid::getBounds(std::uint64_t id) const {
    const auto entry = entries.find(id);
    if (entry == entries.end()) {
        return std::nullopt;
    }

    return entry->second.bounds;
}

void HashGrid::collect(const CellRange& cells, std::vector<std::uint64_t>& ids) const {
    ids.clear();
    if (cells.isEmpty()) {
        return;
    }

    // A range that covers more cells than there are buckets is cheaper to answer by visiting every bucket.
    if (cells.getCount() > buckets.size()) {
        for (const auto& [key, bucket] : buckets) {
            if (cells.contains(cellOfKey(key))) {
                ids.insert(ids.end(), bucket.begin(), bucket.end());
            }
        }
    } else {
        for (int z = cells.low.z; z <= cells.high.z; ++z) {
            for (int y = cells.low.y; y <= cells.high.y; ++y) {
                for (int x = cells.low.x; x <= cells.high.x; ++x) {
                    const auto bucket = buckets.find(cellKey({x, y, z}));
                    if (bucket != buckets.end()) {
                        ids.insert(ids.end(), bucket->second.begin(), bucket->second.end());
                    }
                }
            }
        }
    }

    std::ranges::sort(ids);
    ids.erase(std::ranges::unique(ids).begin(), ids.end());
}

void HashGrid::query(const math::Box& area, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireValid(area);
    collect(occupiedCellsOf(area), ids);
    std::erase_if(ids, [&](std::uint64_t id) { return !entries.at(id).bounds.intersects(area); });
}

void HashGrid::querySphere(const math::Sphere& sphere, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireSphere(sphere);
    collect(occupiedCellsOf(sphere.getBounds()), ids);
    std::erase_if(ids, [&](std::uint64_t id) { return !EntryBounds::touches(entries.at(id).bounds, sphere); });
}

void HashGrid::queryPoint(math::Vec3 point, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requirePoint(point);
    query({point, point}, ids);
}

// A frustum reaches across far more cells than a box query, so the grid tests each entry once instead of walking cells.
void HashGrid::queryFrustum(const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const {
    ids.clear();
    for (const auto& [id, entry] : entries) {
        if (frustum.classify(entry.bounds) != math::Frustum::Containment::Outside) {
            ids.push_back(id);
        }
    }

    std::ranges::sort(ids);
}

void HashGrid::scanCrossed(const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    for (const auto& [id, entry] : entries) {
        EntryBounds::addHit(hits, id, ray, entry.bounds);
    }

    EntryBounds::finishHits(hits, limit);
}

void HashGrid::raycast(const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    hits.clear();
    const std::optional<std::array<float, 2>> travel = entries.empty() ? std::nullopt : EntryBounds::clip(ray, getOccupiedBox());
    if (!travel) {
        return;
    }

    // A ray that crosses more cells than there are entries costs less when it tests every entry.
    const math::Ray3D inside{ray.at((*travel)[0]), ray.direction, (*travel)[1] - (*travel)[0]};
    const math::Vec3 end = inside.getEndPoint();
    const double crossed = std::fabs(static_cast<double>(cellOf(end.x)) - cellOf(inside.origin.x)) + std::fabs(static_cast<double>(cellOf(end.y)) - cellOf(inside.origin.y)) + std::fabs(static_cast<double>(cellOf(end.z)) - cellOf(inside.origin.z)) + 1.0;
    if (crossed > static_cast<double>(entries.size())) {
        scanCrossed(ray, limit, hits);
        return;
    }

    // Cells are visited in the order the ray crosses them, so once the ray enters a cell beyond the last kept hit, no later entry can come closer.
    // clang-format off
    GridRay::traverse(inside, {cellSize, cellSize, cellSize}, [&](Cell cell, float distance, math::Vec3) {
        if (EntryBounds::isSettled(hits, limit, (*travel)[0] + distance)) {
            return false;
        }

        const auto bucket = buckets.find(cellKey(cell));
        if (bucket == buckets.end()) {
            return true;
        }

        for (const std::uint64_t id : bucket->second) {
            if (!EntryBounds::hasHit(hits, id)) {
                EntryBounds::addHit(hits, id, ray, entries.at(id).bounds);
            }
        }

        return true;
    });
    // clang-format on
    EntryBounds::finishHits(hits, limit);
}

void HashGrid::offerBucket(Cell cell, Cell center, math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    if (!occupied.contains(cell)) {
        return;
    }

    const auto bucket = buckets.find(cellKey(cell));
    if (bucket == buckets.end()) {
        return;
    }

    for (const std::uint64_t id : bucket->second) {
        const Entry& entry = entries.at(id);
        const Cell closest{std::clamp(center.x, entry.cells.low.x, entry.cells.high.x), std::clamp(center.y, entry.cells.low.y, entry.cells.high.y), std::clamp(center.z, entry.cells.low.z, entry.cells.high.z)};
        if (cell != closest) {
            continue;
        }

        const float distance = std::sqrt(EntryBounds::distanceSquared(entry.bounds, point));
        if (distance <= maxDistance) {
            EntryBounds::keepNearest(neighbors, {.id = id, .distance = distance}, count);
        }
    }
}

void HashGrid::scanNearest(math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    neighbors.clear();
    for (const auto& [id, entry] : entries) {
        const float distance = std::sqrt(EntryBounds::distanceSquared(entry.bounds, point));
        if (distance <= maxDistance) {
            EntryBounds::keepNearest(neighbors, {.id = id, .distance = distance}, count);
        }
    }

    EntryBounds::finishNearest(neighbors);
}

void HashGrid::nearest(math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    EntryBounds::requirePoint(point);
    EntryBounds::requireRadius(maxDistance);
    neighbors.clear();
    if (count == 0 || entries.empty()) {
        return;
    }

    // Shells of cells grow around the cell of the point. An entry first met in shell `r` misses the inner shells, so it lies at least `r - 1` cells away.
    const std::array<double, 3> pointCell{cellOf(point.x), cellOf(point.y), cellOf(point.z)};
    const std::array<double, 3> low{static_cast<double>(occupied.low.x), static_cast<double>(occupied.low.y), static_cast<double>(occupied.low.z)};
    const std::array<double, 3> high{static_cast<double>(occupied.high.x), static_cast<double>(occupied.high.y), static_cast<double>(occupied.high.z)};
    double gap = 0.0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        gap = std::max({gap, low[axis] - pointCell[axis], pointCell[axis] - high[axis]});
    }

    if ((gap - 1.0) * cellSize > maxDistance) {
        return;
    }

    // A point beyond the occupied cells searches from the cell next to them, which only lowers the bounds of the shells.
    const Cell center{static_cast<int>(std::clamp(pointCell[0], low[0] - 1.0, high[0] + 1.0)), static_cast<int>(std::clamp(pointCell[1], low[1] - 1.0, high[1] + 1.0)), static_cast<int>(std::clamp(pointCell[2], low[2] - 1.0, high[2] + 1.0))};
    const int firstShell = std::max({0, occupied.low.x - center.x, center.x - occupied.high.x, occupied.low.y - center.y, center.y - occupied.high.y, occupied.low.z - center.z, center.z - occupied.high.z});
    const int lastShell = std::max({std::abs(center.x - occupied.low.x), std::abs(center.x - occupied.high.x), std::abs(center.y - occupied.low.y), std::abs(center.y - occupied.high.y), std::abs(center.z - occupied.low.z), std::abs(center.z - occupied.high.z)});
    std::size_t visited = 0;
    for (int shell = firstShell; shell <= lastShell; ++shell) {
        const float nearestPossible = static_cast<float>(shell - 1) * cellSize;
        if (nearestPossible > maxDistance || (neighbors.size() == count && neighbors.front().distance < nearestPossible)) {
            break;
        }

        // Shells that visit more cells than there are entries cost more than measuring every entry.
        const auto side = static_cast<std::size_t>(2 * shell + 1);
        visited += shell == 0 ? 1 : side * side * side - (side - 2) * (side - 2) * (side - 2);
        if (visited > entries.size()) {
            scanNearest(point, count, maxDistance, neighbors);
            return;
        }

        if (shell == 0) {
            offerBucket(center, center, point, count, maxDistance, neighbors);
            continue;
        }

        // The two faces across z hold whole squares, the two across y skip the rows of those faces, and the two across x skip both.
        const int lowX = std::max(center.x - shell, occupied.low.x);
        const int highX = std::min(center.x + shell, occupied.high.x);
        const int lowY = std::max(center.y - shell, occupied.low.y);
        const int highY = std::min(center.y + shell, occupied.high.y);
        for (int y = lowY; y <= highY; ++y) {
            for (int x = lowX; x <= highX; ++x) {
                offerBucket({x, y, center.z - shell}, center, point, count, maxDistance, neighbors);
                offerBucket({x, y, center.z + shell}, center, point, count, maxDistance, neighbors);
            }
        }

        const int lowZ = std::max(center.z - shell + 1, occupied.low.z);
        const int highZ = std::min(center.z + shell - 1, occupied.high.z);
        for (int z = lowZ; z <= highZ; ++z) {
            for (int x = lowX; x <= highX; ++x) {
                offerBucket({x, center.y - shell, z}, center, point, count, maxDistance, neighbors);
                offerBucket({x, center.y + shell, z}, center, point, count, maxDistance, neighbors);
            }

            for (int y = std::max(center.y - shell + 1, occupied.low.y); y <= std::min(center.y + shell - 1, occupied.high.y); ++y) {
                offerBucket({center.x - shell, y, z}, center, point, count, maxDistance, neighbors);
                offerBucket({center.x + shell, y, z}, center, point, count, maxDistance, neighbors);
            }
        }
    }

    EntryBounds::finishNearest(neighbors);
}

} // namespace haylen::spatial3d
