#include "3d/spatial/EntryBounds.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "haylen/math/Geometry3D.hpp"

namespace haylen::spatial3d {

void EntryBounds::requireValid(const math::Box& bounds) {
    if (!bounds.min.isFinite() || !bounds.max.isFinite() || bounds.isEmpty()) {
        throw std::invalid_argument("Spatial bounds must be finite, and no coordinate of their largest corner may lie below the one of their smallest corner.");
    }
}

void EntryBounds::requireRadius(float radius) {
    if (!(radius >= 0.0F)) {
        throw std::invalid_argument("A spatial query radius must not be negative.");
    }
}

void EntryBounds::requirePoint(math::Vec3 point) {
    if (!point.isFinite()) {
        throw std::invalid_argument("A spatial query point must be finite.");
    }
}

void EntryBounds::requireSphere(const math::Sphere& sphere) {
    requirePoint(sphere.center);
    if (!(sphere.radius >= 0.0F) || !std::isfinite(sphere.radius)) {
        throw std::invalid_argument("A spatial sphere needs a finite radius of zero or more.");
    }
}

void EntryBounds::requireBatch(std::size_t ids, std::size_t bounds) {
    if (ids != bounds) {
        throw std::invalid_argument("A batch of spatial entries needs as many bounds as ids.");
    }
}

std::optional<float> EntryBounds::enter(const math::Ray3D& ray, const math::Box& box) noexcept {
    const std::optional<math::RayHit3D> hit = math::Geometry3D::raycast(ray, box);
    if (!hit) {
        return std::nullopt;
    }

    return hit->distance;
}

std::optional<std::array<float, 2>> EntryBounds::clip(const math::Ray3D& ray, const math::Box& box) noexcept {
    const std::array<float, 3> origin{ray.origin.x, ray.origin.y, ray.origin.z};
    const std::array<float, 3> direction{ray.direction.x, ray.direction.y, ray.direction.z};
    const std::array<float, 3> low{box.min.x, box.min.y, box.min.z};
    const std::array<float, 3> high{box.max.x, box.max.y, box.max.z};
    float enter = 0.0F;
    float leave = ray.length;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (direction[axis] == 0.0F) {
            if (origin[axis] < low[axis] || origin[axis] > high[axis]) {
                return std::nullopt;
            }

            continue;
        }

        const float toLow = (low[axis] - origin[axis]) / direction[axis];
        const float toHigh = (high[axis] - origin[axis]) / direction[axis];
        enter = std::max(enter, std::min(toLow, toHigh));
        leave = std::min(leave, std::max(toLow, toHigh));
    }

    if (enter > leave) {
        return std::nullopt;
    }

    return std::array<float, 2>{enter, leave};
}

void EntryBounds::addHit(std::vector<RayHit>& hits, std::uint64_t id, const math::Ray3D& ray, const math::Box& bounds) {
    if (const std::optional<math::RayHit3D> hit = math::Geometry3D::raycast(ray, bounds)) {
        insertHit(hits, {.id = id, .point = hit->point, .normal = hit->normal, .distance = hit->distance});
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

bool EntryBounds::hasHit(const std::vector<RayHit>& hits, std::uint64_t id) noexcept {
    return std::ranges::any_of(hits, [id](const RayHit& hit) { return hit.id == id; });
}

void EntryBounds::finishHits(std::vector<RayHit>& hits, std::size_t limit) {
    if (limit > 0 && hits.size() > limit) {
        hits.resize(limit);
    }
}

} // namespace haylen::spatial3d
