#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "haylen/3d/spatial/BvhNodes.hpp"
#include "haylen/3d/spatial/Neighbor.hpp"
#include "haylen/3d/spatial/RayHit.hpp"
#include "haylen/math/Box.hpp"
#include "haylen/math/Frustum.hpp"
#include "haylen/math/Ray3D.hpp"
#include "haylen/math/Sphere.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::spatial3d {

// A bounding volume hierarchy built once over boxes by the surface area heuristic, the fastest structure to query for content that rarely changes, such as the props of a level. Entries that only move keep the tree with `refit`, and entries that come or go need `build`. Querying a tree changed since its last build or refit throws `std::logic_error`. Queries follow the rules of `HashGrid`.
class Bvh final {
  public:
    void set(std::uint64_t id, const math::Box& bounds);

    // Sets every entry of the batch in order, the same as calling `set` for each id with the bounds at the same position. It checks every entry before it sets any, so a batch with a wrong entry or counts that differ throws `std::invalid_argument` and changes nothing.
    void setAll(std::span<const std::uint64_t> ids, std::span<const math::Box> bounds);
    bool remove(std::uint64_t id);
    void clear() noexcept;

    // Builds the tree from the current entries in time proportional to n log n.
    void build();

    // Fits the boxes of the tree to the moved entries in time proportional to n, keeping its shape. Throws `std::logic_error` when entries came or went since the last build.
    void refit();

    [[nodiscard]] bool isBuilt() const noexcept {
        return state == State::Built;
    }

    [[nodiscard]] bool contains(std::uint64_t id) const;
    [[nodiscard]] std::optional<math::Box> getBounds(std::uint64_t id) const;
    [[nodiscard]] std::size_t size() const noexcept {
        return entries.size();
    }
    [[nodiscard]] std::size_t getNodeCount() const noexcept {
        return tree.size();
    }

    // Returns the number of levels below the root of the last build.
    [[nodiscard]] int getDepth() const noexcept {
        return tree.getDepth();
    }

    void query(const math::Box& area, std::vector<std::uint64_t>& ids) const;
    void querySphere(const math::Sphere& sphere, std::vector<std::uint64_t>& ids) const;
    void queryPoint(math::Vec3 point, std::vector<std::uint64_t>& ids) const;
    void queryFrustum(const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const;
    void raycast(const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const;
    void nearest(math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const;

  private:
    enum class State : std::uint8_t {
        Built,
        Moved,
        Changed,
    };

    struct Entry {
        std::uint64_t id = 0;
        math::Box bounds;
    };

    void requireBuilt() const;

    // Keeps the ids of the entries from `first` on, `count` of them, whose bounds `accepts(bounds)` takes.
    template <typename Accepts> void keep(std::uint32_t first, std::uint32_t count, const Accepts& accepts, std::vector<std::uint64_t>& ids) const;

    // The entries lie in the order of the leaves of the tree after a build.
    std::vector<Entry> entries;
    std::unordered_map<std::uint64_t, std::uint32_t> positions;
    BvhNodes tree;
    State state = State::Built;
};

} // namespace haylen::spatial3d
