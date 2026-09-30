#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "haylen/2d/spatial/Neighbor.hpp"
#include "haylen/2d/spatial/RayHit.hpp"
#include "haylen/math/Ray.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::spatial2d {

// Finds points near other points in a balanced k-d tree, the fastest structure for nearest-neighbor queries over entries that rarely change. Entries are points with an optional radius, which queries treat as circles. Changes take effect when `build` runs, and querying a tree changed since its last build throws `std::logic_error`. Queries follow the rules of `HashGrid`, with distances measured to the circles.
class KdTree final {
  public:
    // Adds an entry, or moves an existing one.
    void set(std::uint64_t id, math::Vec2 point, float radius = 0.0F);
    bool remove(std::uint64_t id);
    void clear() noexcept;

    // Rebuilds the tree from the current entries in time proportional to n log n.
    void build();
    [[nodiscard]] bool isBuilt() const noexcept {
        return built;
    }

    [[nodiscard]] bool contains(std::uint64_t id) const;
    [[nodiscard]] std::optional<math::Vec2> getPoint(std::uint64_t id) const;
    [[nodiscard]] std::size_t size() const noexcept {
        return entries.size();
    }

    void query(const math::Rect& area, std::vector<std::uint64_t>& ids) const;
    void queryCircle(math::Vec2 center, float radius, std::vector<std::uint64_t>& ids) const;
    void queryPoint(math::Vec2 point, std::vector<std::uint64_t>& ids) const;
    void raycast(const math::Ray& ray, std::size_t limit, std::vector<RayHit>& hits) const;
    void nearest(math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

  private:
    struct Entry {
        std::uint64_t id = 0;
        math::Vec2 point{};
        float radius = 0.0F;
    };

    // The part of the tree array a search visits, with the median of the range as its node.
    struct Range {
        std::size_t begin = 0;
        std::size_t end = 0;
        int depth = 0;
    };

    [[nodiscard]] static float along(math::Vec2 point, int depth) noexcept {
        return depth % 2 == 0 ? point.x : point.y;
    }

    void requireBuilt() const;
    void split(Range range);

    // Visits every entry whose circle may touch the box from low to high, pruning the halves the box does not reach. The limits may be infinite.
    template <typename Visit> void visitBox(Range range, math::Vec2 low, math::Vec2 high, Visit& visit) const;
    void collectNearest(Range range, math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

    std::unordered_map<std::uint64_t, Entry> entries;
    std::vector<Entry> tree;
    float maxRadius = 0.0F;
    bool built = true;
};

} // namespace haylen::spatial2d
