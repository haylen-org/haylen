#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "haylen/2d/spatial/Cell.hpp"
#include "haylen/2d/spatial/Neighbor.hpp"
#include "haylen/2d/spatial/RayHit.hpp"
#include "haylen/math/Ray.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::spatial2d {

// Buckets rectangles into a uniform grid so queries only visit nearby entries. Callers pick the ids, and bounds that touch count as overlapping, so point-sized entries are found too. It suits many entries of similar size that move every frame. An entry may cover at most 65536 cells and must lie within 536870912 cells of the origin, and `set` throws `std::invalid_argument` otherwise.
class HashGrid final {
  public:
    explicit HashGrid(float gridCellSize);

    // Adds an entry, or moves an existing one to new bounds.
    void set(std::uint64_t id, const math::Rect& bounds);
    bool remove(std::uint64_t id);
    void clear() noexcept;

    [[nodiscard]] bool contains(std::uint64_t id) const;
    [[nodiscard]] std::optional<math::Rect> getBounds(std::uint64_t id) const;
    [[nodiscard]] std::size_t size() const noexcept {
        return entries.size();
    }
    [[nodiscard]] float getCellSize() const noexcept {
        return cellSize;
    }

    // Queries fill `ids` with each matching id once, in ascending order, so results never depend on insertion history.
    void query(const math::Rect& area, std::vector<std::uint64_t>& ids) const;
    void queryCircle(math::Vec2 center, float radius, std::vector<std::uint64_t>& ids) const;
    void queryPoint(math::Vec2 point, std::vector<std::uint64_t>& ids) const;

    // Fills `hits` with the entries the ray crosses, sorted by distance and then by id, keeping at most `limit` hits unless the limit is zero. The ray only travels across the cells that ever held entries, so its length may be infinite.
    void raycast(const math::Ray& ray, std::size_t limit, std::vector<RayHit>& hits) const;

    // Fills `neighbors` with up to `count` entries whose bounds lie within `maxDistance` of the point, closest first and then by id.
    void nearest(math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

  private:
    struct CellRange {
        int left = 0;
        int top = 0;
        int right = -1;
        int bottom = -1;

        [[nodiscard]] bool operator==(const CellRange&) const noexcept = default;
    };

    struct Entry {
        math::Rect bounds;
        CellRange cells;
    };

    static constexpr float kMaxCell = 536870912.0F;
    static constexpr double kMaxEntryCells = 65536.0;

    [[nodiscard]] static std::uint64_t cellKey(int x, int y) noexcept;

    // Returns the cell of a coordinate as a whole number, which is infinite when the coordinate lies too far for any cell.
    [[nodiscard]] float cellOf(float value) const noexcept;
    [[nodiscard]] CellRange entryCellsOf(const math::Rect& bounds) const;

    // Returns the cells of an area that lie inside the occupied cells, which is empty when the area misses them.
    [[nodiscard]] CellRange occupiedCellsOf(const math::Rect& area) const noexcept;
    void collect(const CellRange& cells, std::vector<std::uint64_t>& ids) const;
    void link(std::uint64_t id, const CellRange& cells);
    void unlink(std::uint64_t id, const CellRange& cells);

    // Offers the entries of one bucket to a nearest query around the center cell, each from the cell of the entry closest to the center only, so every entry comes once.
    void offerBucket(Cell cell, Cell center, math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;
    void scanNearest(math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

    float cellSize;
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> buckets;
    std::unordered_map<std::uint64_t, Entry> entries;

    // The cells that ever held an entry since the last clear, which bounds how far a nearest query searches.
    CellRange occupied;
};

} // namespace haylen::spatial2d
