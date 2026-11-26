#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "haylen/3d/spatial/Neighbor.hpp"
#include "haylen/3d/spatial/RayHit.hpp"
#include "haylen/math/Box.hpp"
#include "haylen/math/Frustum.hpp"
#include "haylen/math/Ray3D.hpp"
#include "haylen/math/Sphere.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::spatial3d {

// Keeps boxes in a balanced binary tree of enlarged boxes, like the broad phase of a physics engine. Moves that stay inside the enlarged box of an entry cost nothing, and queries skip whole branches. It suits many moving entries of any size over unbounded space. Queries follow the rules of `HashGrid`.
class AabbTree final {
  public:
    // The margin enlarges the box the tree keeps for each entry on every side, in meters.
    explicit AabbTree(float boundsMargin = 0.1F);

    // Adds an entry, or moves an existing one to new bounds.
    void set(std::uint64_t id, const math::Box& bounds);

    // Sets every entry of the batch in order, the same as calling `set` for each id with the bounds at the same position. It checks every entry before it sets any, so a batch with a wrong entry or counts that differ throws `std::invalid_argument` and changes nothing.
    void setAll(std::span<const std::uint64_t> ids, std::span<const math::Box> bounds);
    bool remove(std::uint64_t id);
    void clear() noexcept;

    [[nodiscard]] bool contains(std::uint64_t id) const;
    [[nodiscard]] std::optional<math::Box> getBounds(std::uint64_t id) const;
    [[nodiscard]] std::size_t size() const noexcept {
        return leaves.size();
    }
    [[nodiscard]] float getMargin() const noexcept {
        return margin;
    }

    // Returns the number of levels below the root, which balancing keeps near the logarithm of the entry count.
    [[nodiscard]] int getHeight() const noexcept;

    void query(const math::Box& area, std::vector<std::uint64_t>& ids) const;
    void querySphere(const math::Sphere& sphere, std::vector<std::uint64_t>& ids) const;
    void queryPoint(math::Vec3 point, std::vector<std::uint64_t>& ids) const;
    void queryFrustum(const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const;
    void raycast(const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const;
    void nearest(math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

  private:
    struct Node {
        math::Box box;
        math::Box bounds;
        std::int32_t parent = -1;
        std::int32_t left = -1;
        std::int32_t right = -1;
        int height = 0;
        std::uint64_t id = 0;

        [[nodiscard]] bool isLeaf() const noexcept {
            return left < 0;
        }
    };

    // Half the surface area of a box, which grows with the chance that a query meets the box.
    [[nodiscard]] static float halfSurface(const math::Box& box) noexcept {
        const math::Vec3 size = box.getSize();
        return size.x * size.y + size.y * size.z + size.z * size.x;
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

    // Visits the leaves under the nodes that `reaches(box)` accepts and keeps the ids whose bounds `accepts(bounds)` takes.
    template <typename Reaches, typename Accepts> void collect(std::int32_t node, const Reaches& reaches, const Accepts& accepts, std::vector<std::uint64_t>& ids) const;
    void collectAll(std::int32_t node, std::vector<std::uint64_t>& ids) const;
    void collectVisible(std::int32_t node, const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const;
    // Visits the node that the ray enters at `entry`, and its children in the order the ray enters them.
    void collectCrossed(std::int32_t node, float entry, const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const;
    void collectNearest(std::int32_t node, math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

    float margin;
    std::vector<Node> nodes;
    std::int32_t root = -1;
    std::int32_t freeList = -1;
    std::unordered_map<std::uint64_t, std::int32_t> leaves;
};

} // namespace haylen::spatial3d
