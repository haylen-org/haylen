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

// Keeps rectangles in a balanced binary tree of enlarged bounding boxes, like the broad phase of a physics engine. Moves that stay inside the enlarged box of an entry cost nothing, and queries skip whole branches. It suits many moving entries of any size over an unbounded area. Queries follow the rules of `HashGrid`.
class AabbTree final {
  public:
    // The margin enlarges the box the tree keeps for each entry on every side.
    explicit AabbTree(float boundsMargin = 4.0F);

    void set(std::uint64_t id, const math::Rect& bounds);
    bool remove(std::uint64_t id);
    void clear() noexcept;

    [[nodiscard]] bool contains(std::uint64_t id) const;
    [[nodiscard]] std::optional<math::Rect> getBounds(std::uint64_t id) const;
    [[nodiscard]] std::size_t size() const noexcept {
        return leaves.size();
    }
    [[nodiscard]] float getMargin() const noexcept {
        return margin;
    }

    // Returns the number of levels below the root, which balancing keeps near the logarithm of the entry count.
    [[nodiscard]] int getHeight() const noexcept;

    void query(const math::Rect& area, std::vector<std::uint64_t>& ids) const;
    void queryCircle(math::Vec2 center, float radius, std::vector<std::uint64_t>& ids) const;
    void queryPoint(math::Vec2 point, std::vector<std::uint64_t>& ids) const;
    void raycast(const math::Ray& ray, std::size_t limit, std::vector<RayHit>& hits) const;
    void nearest(math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

  private:
    struct Node {
        math::Rect box;
        math::Rect bounds;
        std::int32_t parent = -1;
        std::int32_t left = -1;
        std::int32_t right = -1;
        int height = 0;
        std::uint64_t id = 0;

        [[nodiscard]] bool isLeaf() const noexcept {
            return left < 0;
        }
    };

    [[nodiscard]] static float perimeter(const math::Rect& box) noexcept {
        return 2.0F * (box.width + box.height);
    }

    [[nodiscard]] Node& at(std::int32_t node) noexcept {
        return nodes[static_cast<std::size_t>(node)];
    }
    [[nodiscard]] const Node& at(std::int32_t node) const noexcept {
        return nodes[static_cast<std::size_t>(node)];
    }

    [[nodiscard]] std::int32_t allocate();
    void release(std::int32_t node) noexcept;
    void insertLeaf(std::int32_t leaf);
    void removeLeaf(std::int32_t leaf);

    // Fixes the boxes and heights from a node up to the root, rotating every unbalanced node on the way.
    void refit(std::int32_t node);

    // Rotates a child up when one side of the node is more than one level taller, and returns the node now at its place.
    [[nodiscard]] std::int32_t balance(std::int32_t node);
    void replaceChild(std::int32_t parent, std::int32_t from, std::int32_t to) noexcept;

    void collectOverlapping(std::int32_t node, const math::Rect& area, std::vector<std::uint64_t>& ids) const;
    void collectCrossed(std::int32_t node, const math::Ray& ray, std::size_t limit, std::vector<RayHit>& hits) const;
    void collectNearest(std::int32_t node, math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

    float margin;
    std::vector<Node> nodes;
    std::int32_t root = -1;
    std::int32_t freeList = -1;
    std::unordered_map<std::uint64_t, std::int32_t> leaves;
};

} // namespace haylen::spatial2d
