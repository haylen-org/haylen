#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "haylen/3d/spatial/Neighbor.hpp"
#include "haylen/3d/spatial/RayHit.hpp"
#include "haylen/math/Box.hpp"
#include "haylen/math/Ray3D.hpp"
#include "haylen/math/Sphere.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::spatial3d {

// The rules every 3D spatial structure shares for entry bounds and query results. Bounds that touch count as overlapping, so point-sized entries are found too.
class EntryBounds final {
  public:
    static void requireValid(const math::Box& bounds);
    static void requireRadius(float radius);
    static void requirePoint(math::Vec3 point);
    static void requireSphere(const math::Sphere& sphere);

    // Throws `std::invalid_argument` when a batch gives a different number of ids and bounds.
    static void requireBatch(std::size_t ids, std::size_t bounds);

    [[nodiscard]] static float distanceSquared(const math::Box& bounds, math::Vec3 point) noexcept {
        return math::Vec3::distanceSquared(bounds.clamp(point), point);
    }

    [[nodiscard]] static bool touches(const math::Box& bounds, const math::Sphere& sphere) noexcept {
        return distanceSquared(bounds, sphere.center) <= sphere.radius * sphere.radius;
    }

    // Returns the distance at which the ray enters the box, zero when it starts inside, or nothing when it misses the box before it ends.
    [[nodiscard]] static std::optional<float> enter(const math::Ray3D& ray, const math::Box& box) noexcept;

    // Returns the distances at which the ray enters and leaves the box within its length, or nothing when it misses the box.
    [[nodiscard]] static std::optional<std::array<float, 2>> clip(const math::Ray3D& ray, const math::Box& box) noexcept;

    // Adds the hit of the ray on the bounds of an entry to `hits`, when it hits them.
    static void addHit(std::vector<RayHit>& hits, std::uint64_t id, const math::Ray3D& ray, const math::Box& bounds);

    // Adds a candidate to `neighbors`, which never holds more than `count` entries and keeps them as a heap with the farthest one first until `finishNearest` sorts them.
    static void keepNearest(std::vector<Neighbor>& neighbors, const Neighbor& candidate, std::size_t count);

    // Tells whether a search that keeps `count` neighbors can skip what lies at `distance` or farther.
    [[nodiscard]] static bool isBeyond(const std::vector<Neighbor>& neighbors, std::size_t count, float distance, float maxDistance) noexcept {
        return distance > maxDistance || (neighbors.size() == count && distance > neighbors.front().distance);
    }

    // Sorts the neighbors that `keepNearest` kept by distance and then by id.
    static void finishNearest(std::vector<Neighbor>& neighbors);

    // Adds a hit to `hits`, which stays sorted by distance and then by id.
    static void insertHit(std::vector<RayHit>& hits, const RayHit& hit);

    // Tells whether `hits` already holds `limit` hits that all come before the distance, so nothing farther can make the cut, while a hit at the same distance still can by its id.
    [[nodiscard]] static bool isSettled(const std::vector<RayHit>& hits, std::size_t limit, float distance) noexcept {
        return limit > 0 && hits.size() >= limit && hits[limit - 1].distance < distance;
    }

    [[nodiscard]] static bool hasHit(const std::vector<RayHit>& hits, std::uint64_t id) noexcept;

    // Keeps at most `limit` of the sorted hits unless the limit is zero.
    static void finishHits(std::vector<RayHit>& hits, std::size_t limit);
};

} // namespace haylen::spatial3d
