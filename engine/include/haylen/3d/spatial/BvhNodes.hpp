#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "haylen/math/Box.hpp"
#include "haylen/math/Frustum.hpp"
#include "haylen/math/Geometry3D.hpp"
#include "haylen/math/Ray3D.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::spatial3d {

// The nodes of a bounding volume hierarchy over a list of boxes, built by the surface area heuristic into one array in the order a walk visits them, with the walks that every query of a hierarchy takes. The leaves cover runs of the list in the order that `build` writes, so `Bvh` keeps entries and `MeshBvh` keeps triangles in that order, and a plugin builds its own hierarchy over anything with bounds the same way. Every walk runs on a fixed stack and allocates nothing.
class BvhNodes final {
  public:
    // A leaf covers at most this many boxes.
    static constexpr std::size_t kLeafSize = 4;

    // A node covers the boxes of the list from `first` on, `count` of them, its whole subtree. Its first child follows it in the array, and `second` names its other child, which is zero for a leaf.
    struct Node {
        math::Box box;
        std::uint32_t first = 0;
        std::uint32_t count = 0;
        std::uint32_t second = 0;

        [[nodiscard]] bool isLeaf() const noexcept {
            return second == 0;
        }
    };

    // Builds the nodes over the boxes, which must be finite, and fills `order` with the index of every box in the order of the leaves, in time proportional to n log n. Throws `std::invalid_argument` for more boxes than 32 bits count.
    void build(std::span<const math::Box> boxes, std::vector<std::uint32_t>& order);

    // Fits every node to its boxes again without changing the shape of the tree, where `boundsOf(first, count)` returns the box that holds the boxes of a leaf.
    template <typename BoundsOf> void refit(const BoundsOf& boundsOf) {
        // Children follow their parents in the array, so a walk from the end fits every child before its parent.
        for (std::size_t index = nodes.size(); index-- > 0;) {
            Node& node = nodes[index];
            node.box = node.isLeaf() ? boundsOf(node.first, node.count) : nodes[index + 1].box.merged(nodes[node.second].box);
        }
    }

    void clear() noexcept;

    [[nodiscard]] bool empty() const noexcept {
        return nodes.empty();
    }
    [[nodiscard]] std::size_t size() const noexcept {
        return nodes.size();
    }

    // Returns the box of the root, which holds every box, or nothing for a tree without boxes.
    [[nodiscard]] std::optional<math::Box> getBounds() const noexcept {
        return nodes.empty() ? std::nullopt : std::optional<math::Box>{nodes.front().box};
    }

    // Returns the number of levels below the root.
    [[nodiscard]] int getDepth() const noexcept {
        return depth;
    }

    // Calls `leaf(first, count)` for every leaf under the nodes whose box `reaches(box)` accepts.
    template <typename Reaches, typename Leaf> void visit(const Reaches& reaches, const Leaf& leaf) const {
        Stack stack;
        std::size_t top = 0;
        if (!nodes.empty()) {
            stack[top++] = {};
        }

        while (top > 0) {
            const std::uint32_t index = stack[--top].node;
            const Node& node = nodes[index];
            if (!reaches(node.box)) {
                continue;
            }

            if (node.isLeaf()) {
                leaf(node.first, node.count);
                continue;
            }

            stack[top++] = {.node = node.second};
            stack[top++] = {.node = index + 1};
        }
    }

    // Calls `inside(first, count)` for the boxes of every node that lies wholly inside the frustum and `leaf(first, count)` for every leaf that crosses it, skipping what lies outside.
    template <typename Inside, typename Leaf> void visitFrustum(const math::Frustum& frustum, const Inside& inside, const Leaf& leaf) const {
        Stack stack;
        std::size_t top = 0;
        if (!nodes.empty()) {
            stack[top++] = {};
        }

        while (top > 0) {
            const std::uint32_t index = stack[--top].node;
            const Node& node = nodes[index];
            const math::Frustum::Containment containment = frustum.classify(node.box);
            if (containment == math::Frustum::Containment::Outside) {
                continue;
            }

            if (containment == math::Frustum::Containment::Inside) {
                inside(node.first, node.count);
                continue;
            }

            if (node.isLeaf()) {
                leaf(node.first, node.count);
                continue;
            }

            stack[top++] = {.node = node.second};
            stack[top++] = {.node = index + 1};
        }
    }

    // Calls `leaf(first, count)` for the leaves that the ray crosses, nearest first by where the ray enters them, and skips a node that the ray enters at a distance for which `settled(distance)` is true.
    template <typename Settled, typename Leaf> void visitRay(const math::Ray3D& ray, const Settled& settled, const Leaf& leaf) const {
        Stack stack;
        std::size_t top = 0;
        const std::optional<float> rootEntry = nodes.empty() ? std::nullopt : enter(ray, nodes.front().box);
        if (rootEntry) {
            stack[top++] = {.node = 0, .distance = *rootEntry};
        }

        while (top > 0) {
            const Pending pending = stack[--top];
            if (settled(pending.distance)) {
                continue;
            }

            const Node& node = nodes[pending.node];
            if (node.isLeaf()) {
                leaf(node.first, node.count);
                continue;
            }

            // The child that the ray enters first goes on the stack last, so it is visited first.
            const std::uint32_t left = pending.node + 1;
            const std::optional<float> leftEntry = enter(ray, nodes[left].box);
            const std::optional<float> rightEntry = enter(ray, nodes[node.second].box);
            const bool rightFirst = rightEntry && (!leftEntry || *rightEntry < *leftEntry);
            const std::optional<float> laterEntry = rightFirst ? leftEntry : rightEntry;
            const std::optional<float> soonerEntry = rightFirst ? rightEntry : leftEntry;
            if (laterEntry) {
                stack[top++] = {.node = rightFirst ? left : node.second, .distance = *laterEntry};
            }

            if (soonerEntry) {
                stack[top++] = {.node = rightFirst ? node.second : left, .distance = *soonerEntry};
            }
        }
    }

    // Calls `leaf(first, count)` for the leaves near the point, nearest first by the distance to their boxes, and skips a node at a distance for which `beyond(distance)` is true.
    template <typename Beyond, typename Leaf> void visitNearest(math::Vec3 point, const Beyond& beyond, const Leaf& leaf) const {
        Stack stack;
        std::size_t top = 0;
        if (!nodes.empty()) {
            stack[top++] = {.node = 0, .distance = distanceTo(nodes.front().box, point)};
        }

        while (top > 0) {
            const Pending pending = stack[--top];
            if (beyond(pending.distance)) {
                continue;
            }

            const Node& node = nodes[pending.node];
            if (node.isLeaf()) {
                leaf(node.first, node.count);
                continue;
            }

            const Pending left{.node = pending.node + 1, .distance = distanceTo(nodes[pending.node + 1].box, point)};
            const Pending right{.node = node.second, .distance = distanceTo(nodes[node.second].box, point)};
            const bool leftFirst = left.distance <= right.distance;
            stack[top++] = leftFirst ? right : left;
            stack[top++] = leftFirst ? left : right;
        }
    }

  private:
    // A node that a walk still has to visit, with the distance at which it starts.
    struct Pending {
        std::uint32_t node = 0;
        float distance = 0.0F;
    };

    static constexpr std::size_t kBins = 12;

    // From this level on the build splits every node at its median, so no tree is deeper than this level plus the logarithm of its box count.
    static constexpr int kMedianLevel = 32;
    static constexpr std::size_t kStackSize = 128;

    using Stack = std::array<Pending, kStackSize>;

    [[nodiscard]] static std::optional<float> enter(const math::Ray3D& ray, const math::Box& box) noexcept {
        const std::optional<math::RayHit3D> hit = math::Geometry3D::raycast(ray, box);
        return hit ? std::optional<float>{hit->distance} : std::nullopt;
    }
    [[nodiscard]] static float distanceTo(const math::Box& box, math::Vec3 point) noexcept {
        return math::Vec3::distance(box.clamp(point), point);
    }
    [[nodiscard]] static math::Vec3 centroidOf(const math::Box& bounds) noexcept {
        return bounds.min * 0.5F + bounds.max * 0.5F;
    }
    [[nodiscard]] static float along(math::Vec3 point, int axis) noexcept {
        return axis == 0 ? point.x : (axis == 1 ? point.y : point.z);
    }

    // Half the surface area of a box, which grows with the chance that a query meets the box.
    [[nodiscard]] static float halfSurface(const math::Box& box) noexcept {
        const math::Vec3 size = box.getSize();
        return size.x * size.y + size.y * size.z + size.z * size.x;
    }

    // Builds the node of the boxes of `order` from `first` on, `count` of them, and returns its index.
    std::uint32_t split(std::span<const math::Box> boxes, std::vector<std::uint32_t>& order, std::uint32_t first, std::uint32_t count, int level);

    // Reorders the boxes of a node into two groups and returns the size of the first, by the surface area heuristic, or at the median from `kMedianLevel` on and where the heuristic finds no split.
    [[nodiscard]] static std::uint32_t partition(std::span<const math::Box> boxes, std::vector<std::uint32_t>& order, std::uint32_t first, std::uint32_t count, int level);

    // Puts the half of the boxes whose centers come first along the axis before the other half, and returns the size of that half.
    [[nodiscard]] static std::uint32_t splitAtMedian(std::span<const math::Box> boxes, std::vector<std::uint32_t>& order, std::uint32_t first, std::uint32_t count, int axis);

    std::vector<Node> nodes;
    int depth = 0;
};

} // namespace haylen::spatial3d
