#include "2d/spatial/EntryBounds.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace haylen::spatial2d {

void EntryBounds::requireValid(const math::Rect& bounds) {
    const bool finite = std::isfinite(bounds.x) && std::isfinite(bounds.y) && std::isfinite(bounds.width) && std::isfinite(bounds.height);
    if (!finite || bounds.width < 0.0F || bounds.height < 0.0F) {
        throw std::invalid_argument("Spatial bounds must be finite and have a non-negative size.");
    }
}

void EntryBounds::requireRadius(float radius) {
    if (!(radius >= 0.0F)) {
        throw std::invalid_argument("A spatial query radius must not be negative.");
    }
}

void EntryBounds::keepNearest(std::vector<Neighbor>& neighbors, const Neighbor& candidate, std::size_t count) {
    if (count == 0 || (neighbors.size() == count && !(candidate < neighbors.back()))) {
        return;
    }
    if (neighbors.size() == count) {
        neighbors.pop_back();
    }
    neighbors.insert(std::upper_bound(neighbors.begin(), neighbors.end(), candidate), candidate);
}

void EntryBounds::insertHit(std::vector<RayHit>& hits, const RayHit& hit) {
    const auto closer = [](const RayHit& lhs, const RayHit& rhs) { return lhs.distance != rhs.distance ? lhs.distance < rhs.distance : lhs.id < rhs.id; };
    hits.insert(std::ranges::upper_bound(hits, hit, closer), hit);
}

void EntryBounds::finishHits(std::vector<RayHit>& hits, std::size_t limit) {
    if (limit > 0 && hits.size() > limit) {
        hits.resize(limit);
    }
}

} // namespace haylen::spatial2d
