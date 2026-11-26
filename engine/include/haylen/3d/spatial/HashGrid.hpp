#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "haylen/3d/spatial/Cell.hpp"
#include "haylen/3d/spatial/Neighbor.hpp"
#include "haylen/3d/spatial/RayHit.hpp"
#include "haylen/math/Box.hpp"
#include "haylen/math/Frustum.hpp"
#include "haylen/math/Ray3D.hpp"
#include "haylen/math/Sphere.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::spatial3d {

// Buckets boxes into a uniform grid of cubic cells so queries only visit nearby entries. Callers pick the ids, and bounds that touch count as overlapping, so point-sized entries are found too. It suits many entries of similar size that move every frame, such as particles, crowds or bullets. An entry may cover at most 65536 cells and must lie within 1048575 cells of the origin, and `set` throws `std::invalid_argument` otherwise.
class HashGrid final {
  public:
    // The cell size is the edge of every cubic cell, in meters.
    explicit HashGrid(float gridCellSize);

    // Adds an entry, or moves an existing one to new bounds.
    void set(std::uint64_t id, const math::Box& bounds);

    // Sets every entry of the batch in order, the same as calling `set` for each id with the bounds at the same position. It checks every entry before it sets any, so a batch with a wrong entry or counts that differ throws `std::invalid_argument` and changes nothing.
    void setAll(std::span<const std::uint64_t> ids, std::span<const math::Box> bounds);
    bool remove(std::uint64_t id);
    void clear() noexcept;

    [[nodiscard]] bool contains(std::uint64_t id) const;
    [[nodiscard]] std::optional<math::Box> getBounds(std::uint64_t id) const;
    [[nodiscard]] std::size_t size() const noexcept {
        return entries.size();
    }
    [[nodiscard]] float getCellSize() const noexcept {
        return cellSize;
    }

    // Queries fill `ids` with each matching id once, in ascending order, so results never depend on insertion history.
    void query(const math::Box& area, std::vector<std::uint64_t>& ids) const;
    void querySphere(const math::Sphere& sphere, std::vector<std::uint64_t>& ids) const;
    void queryPoint(math::Vec3 point, std::vector<std::uint64_t>& ids) const;

    // Fills `ids` with the entries whose bounds the frustum does not classify as outside, testing each entry once, since a frustum reaches across far more cells than its entries fill.
    void queryFrustum(const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const;

    // Fills `hits` with the entries the ray crosses, sorted by distance and then by id, keeping at most `limit` hits unless the limit is zero. The ray only travels across the cells that ever held entries, so its length may be infinite.
    void raycast(const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const;

    // Fills `neighbors` with up to `count` entries whose bounds lie within `maxDistance` of the point, closest first and then by id.
    void nearest(math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

  private:
    struct CellRange {
        Cell low;
        Cell high{-1, -1, -1};

        [[nodiscard]] bool operator==(const CellRange&) const noexcept = default;

        [[nodiscard]] bool isEmpty() const noexcept {
            return high.x < low.x || high.y < low.y || high.z < low.z;
        }
        [[nodiscard]] bool contains(Cell cell) const noexcept {
            return cell.x >= low.x && cell.x <= high.x && cell.y >= low.y && cell.y <= high.y && cell.z >= low.z && cell.z <= high.z;
        }
        [[nodiscard]] std::uint64_t getCount() const noexcept {
            return static_cast<std::uint64_t>(std::int64_t{high.x} - low.x + 1) * static_cast<std::uint64_t>(std::int64_t{high.y} - low.y + 1) * static_cast<std::uint64_t>(std::int64_t{high.z} - low.z + 1);
        }
    };

    struct Entry {
        math::Box bounds;
        CellRange cells;
    };

    static constexpr float kMaxCell = 1048575.0F;
    static constexpr std::uint32_t kCellOffset = 1048576U;
    static constexpr std::uint64_t kMaxEntryCells = 65536U;

    [[nodiscard]] static std::uint64_t cellKey(Cell cell) noexcept;
    [[nodiscard]] static Cell cellOfKey(std::uint64_t key) noexcept;

    // Returns the cell of a coordinate as a whole number, which is infinite when the coordinate lies too far for any cell.
    [[nodiscard]] float cellOf(float value) const noexcept;
    [[nodiscard]] std::array<float, 6> getCellLimits(const math::Box& bounds) const noexcept;

    // Throws `std::invalid_argument` for bounds that are not valid or cover too many cells or cells too far away.
    void requireCells(const math::Box& bounds) const;
    [[nodiscard]] CellRange entryCellsOf(const math::Box& bounds) const;

    // Returns the cells of an area that lie inside the occupied cells, which is empty when the area misses them.
    [[nodiscard]] CellRange occupiedCellsOf(const math::Box& area) const noexcept;
    [[nodiscard]] math::Box getOccupiedBox() const noexcept;

    // Gathers the ids of the cells into `ids`, each once and in ascending order.
    void collect(const CellRange& cells, std::vector<std::uint64_t>& ids) const;
    void link(std::uint64_t id, const CellRange& cells);
    void unlink(std::uint64_t id, const CellRange& cells);

    // Offers the entries of one bucket to a nearest query around the center cell, each from the cell of the entry closest to the center only, so every entry comes once.
    void offerBucket(Cell cell, Cell center, math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;
    void scanNearest(math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;
    void scanCrossed(const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const;

    float cellSize;
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> buckets;
    std::unordered_map<std::uint64_t, Entry> entries;

    // The cells that ever held an entry since the last clear, which bounds how far a nearest query or a ray searches.
    CellRange occupied;
};

} // namespace haylen::spatial3d
