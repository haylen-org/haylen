#include "haylen/3d/spatial/KdTree.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "3d/spatial/EntryBounds.hpp"
#include "haylen/math/Geometry3D.hpp"

namespace haylen::spatial3d {

void KdTree::requireEntry(math::Vec3 point, float radius) {
    if (!point.isFinite() || !(radius >= 0.0F) || !std::isfinite(radius)) {
        throw std::invalid_argument("A k-d tree entry needs a finite point and a finite radius of zero or more.");
    }
}

void KdTree::set(std::uint64_t id, math::Vec3 point, float radius) {
    requireEntry(point, radius);
    entries.insert_or_assign(id, Entry{.id = id, .sphere = {point, radius}});
    built = false;
}

void KdTree::setAll(std::span<const std::uint64_t> ids, std::span<const math::Sphere> spheres) {
    EntryBounds::requireBatch(ids.size(), spheres.size());
    for (const math::Sphere& sphere : spheres) {
        requireEntry(sphere.center, sphere.radius);
    }

    for (std::size_t index = 0; index < ids.size(); ++index) {
        set(ids[index], spheres[index].center, spheres[index].radius);
    }
}

bool KdTree::remove(std::uint64_t id) {
    if (entries.erase(id) == 0) {
        return false;
    }

    built = false;
    return true;
}

void KdTree::clear() noexcept {
    entries.clear();
    tree.clear();
    extent = {};
    maxRadius = 0.0F;
    built = true;
}

void KdTree::build() {
    tree.clear();
    tree.reserve(entries.size());
    maxRadius = 0.0F;
    for (const auto& [id, entry] : entries) {
        tree.push_back(entry);
        maxRadius = std::max(maxRadius, entry.sphere.radius);
        extent = tree.size() == 1 ? entry.sphere.getBounds() : extent.merged(entry.sphere.getBounds());
    }

    // Sorting by id first makes the tree independent of the order the hash map keeps.
    std::ranges::sort(tree, [](const Entry& lhs, const Entry& rhs) { return lhs.id < rhs.id; });
    split({.begin = 0, .end = tree.size(), .depth = 0});
    built = true;
}

void KdTree::split(Range range) {
    if (range.end - range.begin <= 1) {
        return;
    }

    const auto begin = tree.begin() + static_cast<std::ptrdiff_t>(range.begin);
    // clang-format off
    std::nth_element(begin, tree.begin() + static_cast<std::ptrdiff_t>(range.getMiddle()), tree.begin() + static_cast<std::ptrdiff_t>(range.end), [depth = range.depth](const Entry& lhs, const Entry& rhs) {
        const float left = along(lhs.sphere.center, depth);
        const float right = along(rhs.sphere.center, depth);
        return left != right ? left < right : lhs.id < rhs.id;
    });
    // clang-format on
    split(range.getLower());
    split(range.getUpper());
}

void KdTree::setAlong(math::Vec3& point, int depth, float value) noexcept {
    const int axis = depth % 3;
    (axis == 0 ? point.x : (axis == 1 ? point.y : point.z)) = value;
}

void KdTree::requireBuilt() const {
    if (!built) {
        throw std::logic_error("The k-d tree changed since it was last built, so it needs \"build\" before a query.");
    }
}

bool KdTree::contains(std::uint64_t id) const {
    return entries.contains(id);
}

std::optional<math::Sphere> KdTree::getSphere(std::uint64_t id) const {
    const auto entry = entries.find(id);
    if (entry == entries.end()) {
        return std::nullopt;
    }

    return entry->second.sphere;
}

template <typename Visit> void KdTree::visitBox(Range range, math::Vec3 low, math::Vec3 high, Visit& visit) const {
    if (range.begin >= range.end) {
        return;
    }

    const Entry& entry = tree[range.getMiddle()];
    visit(entry);

    // The lower half holds entries up to the plane of the node and the upper half entries from it, and each sphere reaches `maxRadius` past its center at most.
    const float plane = along(entry.sphere.center, range.depth);
    if (along(low, range.depth) - maxRadius <= plane) {
        visitBox(range.getLower(), low, high, visit);
    }

    if (along(high, range.depth) + maxRadius >= plane) {
        visitBox(range.getUpper(), low, high, visit);
    }
}

void KdTree::query(const math::Box& area, std::vector<std::uint64_t>& ids) const {
    requireBuilt();
    EntryBounds::requireValid(area);
    ids.clear();
    // clang-format off
    const auto visit = [&](const Entry& entry) {
        if (EntryBounds::touches(area, entry.sphere)) {
            ids.push_back(entry.id);
        }
    };
    // clang-format on
    visitBox({.begin = 0, .end = tree.size()}, area.min, area.max, visit);
    std::ranges::sort(ids);
}

void KdTree::querySphere(const math::Sphere& sphere, std::vector<std::uint64_t>& ids) const {
    requireBuilt();
    EntryBounds::requireSphere(sphere);
    ids.clear();
    // clang-format off
    const auto visit = [&](const Entry& entry) {
        if (sphere.intersects(entry.sphere)) {
            ids.push_back(entry.id);
        }
    };
    // clang-format on
    const math::Box box = sphere.getBounds();
    visitBox({.begin = 0, .end = tree.size()}, box.min, box.max, visit);
    std::ranges::sort(ids);
}

void KdTree::queryPoint(math::Vec3 point, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requirePoint(point);
    querySphere({point, 0.0F}, ids);
}

void KdTree::collectVisible(Range range, const math::Box& region, const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const {
    if (range.begin >= range.end) {
        return;
    }

    // A region wholly inside the frustum holds only spheres inside it, which lie next to each other in the array, so they join without a test.
    const math::Frustum::Containment containment = frustum.classify(region);
    if (containment == math::Frustum::Containment::Outside) {
        return;
    }

    if (containment == math::Frustum::Containment::Inside) {
        for (std::size_t index = range.begin; index < range.end; ++index) {
            ids.push_back(tree[index].id);
        }

        return;
    }

    const Entry& entry = tree[range.getMiddle()];
    if (frustum.classify(entry.sphere) != math::Frustum::Containment::Outside) {
        ids.push_back(entry.id);
    }

    const auto [lower, upper] = splitRegion(range, region);
    collectVisible(range.getLower(), lower, frustum, ids);
    collectVisible(range.getUpper(), upper, frustum, ids);
}

// The spheres of the lower half reach at most `maxRadius` past the plane of the node, and those of the upper half at most `maxRadius` before it.
std::pair<math::Box, math::Box> KdTree::splitRegion(Range range, const math::Box& region) const noexcept {
    const float plane = along(tree[range.getMiddle()].sphere.center, range.depth);
    math::Box lower = region;
    setAlong(lower.max, range.depth, std::min(along(region.max, range.depth), plane + maxRadius));
    math::Box upper = region;
    setAlong(upper.min, range.depth, std::max(along(region.min, range.depth), plane - maxRadius));
    return {lower, upper};
}

void KdTree::queryFrustum(const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const {
    requireBuilt();
    ids.clear();
    collectVisible({.begin = 0, .end = tree.size()}, extent, frustum, ids);
    std::ranges::sort(ids);
}

void KdTree::collectCrossed(Range range, const math::Box& region, float entry, const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    if (range.begin >= range.end || EntryBounds::isSettled(hits, limit, entry)) {
        return;
    }

    const Entry& node = tree[range.getMiddle()];
    if (const std::optional<math::RayHit3D> hit = math::Geometry3D::raycast(ray, node.sphere)) {
        EntryBounds::insertHit(hits, {.id = node.id, .point = hit->point, .normal = hit->normal, .distance = hit->distance});
    }

    const auto [lower, upper] = splitRegion(range, region);
    const std::optional<float> lowerEntry = EntryBounds::enter(ray, lower);
    const std::optional<float> upperEntry = EntryBounds::enter(ray, upper);
    const bool upperFirst = upperEntry && (!lowerEntry || *upperEntry < *lowerEntry);
    const std::optional<float> soonerEntry = upperFirst ? upperEntry : lowerEntry;
    const std::optional<float> laterEntry = upperFirst ? lowerEntry : upperEntry;
    if (soonerEntry) {
        collectCrossed(upperFirst ? range.getUpper() : range.getLower(), upperFirst ? upper : lower, *soonerEntry, ray, limit, hits);
    }

    if (laterEntry) {
        collectCrossed(upperFirst ? range.getLower() : range.getUpper(), upperFirst ? lower : upper, *laterEntry, ray, limit, hits);
    }
}

// The ray walks the regions of the halves of the tree in the order it enters them, so a hit near its origin settles the regions beyond it.
void KdTree::raycast(const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    requireBuilt();
    hits.clear();
    const std::optional<float> entry = tree.empty() ? std::nullopt : EntryBounds::enter(ray, extent);
    if (entry) {
        collectCrossed({.begin = 0, .end = tree.size()}, extent, *entry, ray, limit, hits);
    }

    EntryBounds::finishHits(hits, limit);
}

void KdTree::collectNearest(Range range, math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    if (range.begin >= range.end) {
        return;
    }

    const Entry& entry = tree[range.getMiddle()];
    const float distance = std::max(0.0F, math::Vec3::distance(point, entry.sphere.center) - entry.sphere.radius);
    if (distance <= maxDistance) {
        EntryBounds::keepNearest(neighbors, {.id = entry.id, .distance = distance}, count);
    }

    // The half on the side of the point goes first, and the other half only when the split plane is closer than the farthest neighbor kept.
    const float offset = along(point, range.depth) - along(entry.sphere.center, range.depth);
    collectNearest(offset <= 0.0F ? range.getLower() : range.getUpper(), point, count, maxDistance, neighbors);
    const float planeDistance = std::fabs(offset) - maxRadius;
    if (!EntryBounds::isBeyond(neighbors, count, planeDistance, maxDistance)) {
        collectNearest(offset <= 0.0F ? range.getUpper() : range.getLower(), point, count, maxDistance, neighbors);
    }
}

void KdTree::nearest(math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    requireBuilt();
    EntryBounds::requirePoint(point);
    EntryBounds::requireRadius(maxDistance);
    neighbors.clear();
    if (count > 0) {
        collectNearest({.begin = 0, .end = tree.size()}, point, count, maxDistance, neighbors);
    }

    EntryBounds::finishNearest(neighbors);
}

} // namespace haylen::spatial3d
