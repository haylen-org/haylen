#pragma once

#include <cstddef>
#include <vector>

#include "haylen/2d/spatial/Neighbor.hpp"
#include "haylen/2d/spatial/RayHit.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::spatial2d {

// The rules every spatial structure shares for entry bounds and query results. Bounds that touch count as overlapping, so point-sized entries are found too.
class EntryBounds final {
  public:
    static void requireValid(const math::Rect& bounds);
    static void requireRadius(float radius);
    static void requirePoint(math::Vec2 point);

    [[nodiscard]] static bool overlaps(const math::Rect& lhs, const math::Rect& rhs) noexcept {
        return lhs.getLeft() <= rhs.getRight() && rhs.getLeft() <= lhs.getRight() && lhs.getTop() <= rhs.getBottom() && rhs.getTop() <= lhs.getBottom();
    }

    [[nodiscard]] static float distanceSquared(const math::Rect& bounds, math::Vec2 point) noexcept {
        return math::Vec2::distanceSquared(bounds.clamp(point), point);
    }

    // Adds a candidate to `neighbors`, which never holds more than `count` entries and keeps them as a heap with the farthest one first until `finishNearest` sorts them.
    static void keepNearest(std::vector<Neighbor>& neighbors, const Neighbor& candidate, std::size_t count);

    // Returns the farthest of the neighbors kept so far, which must not be empty.
    [[nodiscard]] static const Neighbor& getFarthest(const std::vector<Neighbor>& neighbors) noexcept {
        return neighbors.front();
    }

    // Sorts the neighbors that `keepNearest` kept by distance and then by id.
    static void finishNearest(std::vector<Neighbor>& neighbors);

    // Adds a hit to `hits`, which stays sorted by distance and then by id.
    static void insertHit(std::vector<RayHit>& hits, const RayHit& hit);

    // Tells whether `hits` already holds `limit` hits that all come before the distance, so nothing farther can make the cut.
    [[nodiscard]] static bool isSettled(const std::vector<RayHit>& hits, std::size_t limit, float distance) noexcept {
        return limit > 0 && hits.size() >= limit && hits[limit - 1].distance <= distance;
    }

    // Keeps at most `limit` of the sorted hits unless the limit is zero.
    static void finishHits(std::vector<RayHit>& hits, std::size_t limit);
};

} // namespace haylen::spatial2d
