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

// Stores rectangles in a tree of quadrants over a fixed area, splitting a quadrant when it holds too many entries and merging quadrants again when they empty. Entries live in the deepest quadrant that contains them whole, and entries outside the area stay at the root. It suits entries of varied sizes spread over a known area. Queries follow the rules of `HashGrid`.
class QuadTree final {
  public:
    static constexpr int kMaxDepth = 16;

    struct Settings {
        std::size_t maxEntries = 8;
        int maxDepth = 8;
    };

    // Throws `std::invalid_argument` for an empty area, a `maxEntries` of 0 or a `maxDepth` outside 0 to `kMaxDepth`.
    explicit QuadTree(const math::Rect& area, const Settings& settings = kDefaultSettings);

    void set(std::uint64_t id, const math::Rect& bounds);
    bool remove(std::uint64_t id);
    void clear();

    [[nodiscard]] bool contains(std::uint64_t id) const;
    [[nodiscard]] std::optional<math::Rect> getBounds(std::uint64_t id) const;
    [[nodiscard]] std::size_t size() const noexcept {
        return entries.size();
    }
    [[nodiscard]] const math::Rect& getArea() const noexcept {
        return nodes.front().bounds;
    }

    // Returns the number of quadrants in use, including the root.
    [[nodiscard]] std::size_t getNodeCount() const noexcept;

    void query(const math::Rect& area, std::vector<std::uint64_t>& ids) const;
    void queryCircle(math::Vec2 center, float radius, std::vector<std::uint64_t>& ids) const;
    void queryPoint(math::Vec2 point, std::vector<std::uint64_t>& ids) const;
    void raycast(const math::Ray& ray, std::size_t limit, std::vector<RayHit>& hits) const;
    void nearest(math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

  private:
    struct Node {
        math::Rect bounds;
        std::int32_t parent = -1;
        std::int32_t firstChild = -1;
        int depth = 0;
        std::vector<std::uint64_t> ids;
    };

    struct Entry {
        math::Rect bounds;
        std::int32_t node = 0;
    };

    static const Settings kDefaultSettings;

    // Returns the child quadrant that contains the bounds whole, or -1 when they straddle the children or the node is a leaf.
    [[nodiscard]] std::int32_t childFor(std::int32_t node, const math::Rect& bounds) const noexcept;
    void insert(std::uint64_t id, const math::Rect& bounds);
    void split(std::int32_t node);

    // Folds the children of a node back into it, and then its ancestors, while their subtrees hold few enough entries.
    void collapse(std::int32_t node);
    [[nodiscard]] std::size_t countBelow(std::int32_t node) const noexcept;
    void gather(std::int32_t node, std::vector<std::uint64_t>& ids) const;

    // Frees the quadrant blocks below a node, which becomes a leaf.
    void release(std::int32_t node);

    void collectOverlapping(std::int32_t node, const math::Rect& area, std::vector<std::uint64_t>& ids) const;
    void collectCrossed(std::int32_t node, const math::Ray& ray, std::size_t limit, std::vector<RayHit>& hits) const;
    void collectNearest(std::int32_t node, math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

    Settings settings;
    std::vector<Node> nodes;
    std::vector<std::int32_t> freeBlocks;
    std::unordered_map<std::uint64_t, Entry> entries;
};

} // namespace haylen::spatial2d
