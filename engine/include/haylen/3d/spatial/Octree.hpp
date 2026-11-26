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

// Stores boxes in a loose octree over a fixed box, splitting a node into eight when it holds too many entries and merging nodes again when they empty. Each node reaches past its own cell by `looseness` times half the size of the cell on every side, so an entry goes down to the deepest node whose reach holds it whole, chosen by the octant of its center, and entries outside the box stay at the root. It suits entries of varied sizes spread over a known region, such as the objects of a level. Queries follow the rules of `HashGrid`.
class Octree final {
  public:
    static constexpr int kMaxDepth = 16;

    struct Settings {
        std::size_t maxEntries = 8;
        int maxDepth = 8;
        float looseness = 1.0F;
    };

    // Throws `std::invalid_argument` for a box without volume, a `maxEntries` of 0, a `maxDepth` outside 0 to `kMaxDepth` or a `looseness` outside 0 to 4.
    explicit Octree(const math::Box& region, const Settings& value = kDefaultSettings);

    void set(std::uint64_t id, const math::Box& bounds);

    // Sets every entry of the batch in order, the same as calling `set` for each id with the bounds at the same position. It checks every entry before it sets any, so a batch with a wrong entry or counts that differ throws `std::invalid_argument` and changes nothing.
    void setAll(std::span<const std::uint64_t> ids, std::span<const math::Box> bounds);
    bool remove(std::uint64_t id);
    void clear();

    [[nodiscard]] bool contains(std::uint64_t id) const;
    [[nodiscard]] std::optional<math::Box> getBounds(std::uint64_t id) const;
    [[nodiscard]] std::size_t size() const noexcept {
        return entries.size();
    }
    [[nodiscard]] const math::Box& getRegion() const noexcept {
        return nodes.front().cell;
    }
    [[nodiscard]] const Settings& getSettings() const noexcept {
        return settings;
    }

    // Returns the number of nodes in use, including the root.
    [[nodiscard]] std::size_t getNodeCount() const noexcept;

    void query(const math::Box& area, std::vector<std::uint64_t>& ids) const;
    void querySphere(const math::Sphere& sphere, std::vector<std::uint64_t>& ids) const;
    void queryPoint(math::Vec3 point, std::vector<std::uint64_t>& ids) const;
    void queryFrustum(const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const;
    void raycast(const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const;
    void nearest(math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

  private:
    struct Node {
        math::Box cell;
        math::Box reach;
        std::int32_t parent = -1;
        std::int32_t firstChild = -1;
        int depth = 0;
        std::vector<std::uint64_t> ids{};
    };

    struct Entry {
        math::Box bounds;
        std::int32_t node = 0;
    };

    static constexpr std::int32_t kChildren = 8;
    static constexpr float kMaxLooseness = 4.0F;
    static const Settings kDefaultSettings;

    [[nodiscard]] Node& at(std::int32_t node) noexcept {
        return nodes[static_cast<std::size_t>(node)];
    }
    [[nodiscard]] const Node& at(std::int32_t node) const noexcept {
        return nodes[static_cast<std::size_t>(node)];
    }

    // Returns the child in whose octant the center of the bounds lies when the reach of that child holds them whole, or -1 when it does not or the node is a leaf.
    [[nodiscard]] std::int32_t childFor(std::int32_t node, const math::Box& bounds) const noexcept;
    void insert(std::uint64_t id, const math::Box& bounds);
    void split(std::int32_t node);

    // Folds the children of a node back into it, and then its ancestors, while their subtrees hold few enough entries.
    void collapse(std::int32_t node);
    [[nodiscard]] std::size_t countBelow(std::int32_t node) const noexcept;
    void gather(std::int32_t node, std::vector<std::uint64_t>& ids) const;

    // Frees the blocks of children below a node, which becomes a leaf.
    void release(std::int32_t node);

    // Visits the nodes whose reach `reaches(box)` accepts and keeps the ids whose bounds `accepts(bounds)` takes.
    template <typename Reaches, typename Accepts> void collect(std::int32_t node, const Reaches& reaches, const Accepts& accepts, std::vector<std::uint64_t>& ids) const;
    void collectAll(std::int32_t node, std::vector<std::uint64_t>& ids) const;
    void collectVisible(std::int32_t node, const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const;
    void collectCrossed(std::int32_t node, const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const;
    void collectNearest(std::int32_t node, math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

    Settings settings;
    std::vector<Node> nodes;
    std::vector<std::int32_t> freeBlocks;
    std::unordered_map<std::uint64_t, Entry> entries;
};

} // namespace haylen::spatial3d
