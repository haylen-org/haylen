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

void EntryBounds::requirePoint(math::Vec2 point) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        throw std::invalid_argument("A spatial query point must be finite.");
    }
}

void EntryBounds::keepNearest(std::vector<Neighbor>& neighbors, const Neighbor& candidate, std::size_t count) {
    if (count == 0 || (neighbors.size() == count && !(candidate < neighbors.front()))) {
        return;
    }
    if (neighbors.size() == count) {
        std::pop_heap(neighbors.begin(), neighbors.end());
        neighbors.pop_back();
    }
    neighbors.push_back(candidate);
    std::push_heap(neighbors.begin(), neighbors.end());
}

void EntryBounds::finishNearest(std::vector<Neighbor>& neighbors) {
    std::sort_heap(neighbors.begin(), neighbors.end());
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
