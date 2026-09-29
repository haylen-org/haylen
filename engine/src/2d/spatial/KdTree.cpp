#include "haylen/2d/spatial/KdTree.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "2d/spatial/EntryBounds.hpp"
#include "haylen/math/Circle.hpp"
#include "haylen/math/Raycast.hpp"

namespace haylen::spatial2d {

void KdTree::set(std::uint64_t id, math::Vec2 point, float radius) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !(radius >= 0.0F) || !std::isfinite(radius)) {
        throw std::invalid_argument("A k-d tree entry needs a finite point and a finite radius of zero or more.");
    }
    entries.insert_or_assign(id, Entry{.id = id, .point = point, .radius = radius});
    built = false;
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
    maxRadius = 0.0F;
    built = true;
}

void KdTree::build() {
    tree.clear();
    tree.reserve(entries.size());
    maxRadius = 0.0F;
    for (const auto& [id, entry] : entries) {
        tree.push_back(entry);
        maxRadius = std::max(maxRadius, entry.radius);
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
    const std::size_t middle = range.begin + (range.end - range.begin) / 2;
    // clang-format off
    std::nth_element(tree.begin() + static_cast<std::ptrdiff_t>(range.begin), tree.begin() + static_cast<std::ptrdiff_t>(middle), tree.begin() + static_cast<std::ptrdiff_t>(range.end), [depth = range.depth](const Entry& lhs, const Entry& rhs) {
        const float left = along(lhs.point, depth);
        const float right = along(rhs.point, depth);
        return left != right ? left < right : lhs.id < rhs.id;
    });
    // clang-format on
    split({.begin = range.begin, .end = middle, .depth = range.depth + 1});
    split({.begin = middle + 1, .end = range.end, .depth = range.depth + 1});
}

void KdTree::requireBuilt() const {
    if (!built) {
        throw std::logic_error("The k-d tree changed since it was last built, so it needs build before a query.");
    }
}

bool KdTree::contains(std::uint64_t id) const {
    return entries.contains(id);
}

std::optional<math::Vec2> KdTree::getPoint(std::uint64_t id) const {
    const auto entry = entries.find(id);
    if (entry == entries.end()) {
        return std::nullopt;
    }
    return entry->second.point;
}

template <typename Visit> void KdTree::visitBox(Range range, math::Vec2 low, math::Vec2 high, Visit& visit) const {
    if (range.begin >= range.end) {
        return;
    }
    const std::size_t middle = range.begin + (range.end - range.begin) / 2;
    const Entry& entry = tree[middle];
    visit(entry);

    // The lower half holds entries up to the split and the upper half entries from it, and each circle reaches maxRadius past its center at most.
    const float split = along(entry.point, range.depth);
    if (along(low, range.depth) - maxRadius <= split) {
        visitBox({.begin = range.begin, .end = middle, .depth = range.depth + 1}, low, high, visit);
    }
    if (along(high, range.depth) + maxRadius >= split) {
        visitBox({.begin = middle + 1, .end = range.end, .depth = range.depth + 1}, low, high, visit);
    }
}

void KdTree::query(const math::Rect& area, std::vector<std::uint64_t>& ids) const {
    requireBuilt();
    EntryBounds::requireValid(area);
    ids.clear();
    // clang-format off
    const auto visit = [&](const Entry& entry) {
        if (EntryBounds::distanceSquared(area, entry.point) <= entry.radius * entry.radius) {
            ids.push_back(entry.id);
        }
    };
    // clang-format on
    visitBox({.begin = 0, .end = tree.size()}, area.getMin(), area.getMax(), visit);
    std::ranges::sort(ids);
}

void KdTree::queryCircle(math::Vec2 center, float radius, std::vector<std::uint64_t>& ids) const {
    requireBuilt();
    EntryBounds::requireRadius(radius);
    const math::Rect box = math::Rect::fromCenter(center, {radius * 2.0F, radius * 2.0F});
    EntryBounds::requireValid(box);
    ids.clear();
    // clang-format off
    const auto visit = [&](const Entry& entry) {
        if (math::Vec2::distance(center, entry.point) <= radius + entry.radius) {
            ids.push_back(entry.id);
        }
    };
    // clang-format on
    visitBox({.begin = 0, .end = tree.size()}, box.getMin(), box.getMax(), visit);
    std::ranges::sort(ids);
}

void KdTree::queryPoint(math::Vec2 point, std::vector<std::uint64_t>& ids) const {
    queryCircle(point, 0.0F, ids);
}

void KdTree::raycast(const math::Ray& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    requireBuilt();
    hits.clear();

    // A ray without an end reaches infinity along the axes it moves on, which the box search takes as limits.
    const float infinity = std::numeric_limits<float>::infinity();
    math::Vec2 low = ray.origin;
    math::Vec2 high = ray.origin;
    for (float math::Vec2::* axis : {&math::Vec2::x, &math::Vec2::y}) {
        const float direction = ray.direction.*axis;
        const float end = std::isfinite(ray.length) ? ray.origin.*axis + direction * ray.length : (direction > 0.0F ? infinity : -infinity);
        if (direction != 0.0F) {
            low.*axis = std::min(low.*axis, end);
            high.*axis = std::max(high.*axis, end);
        }
    }

    // clang-format off
    const auto visit = [&](const Entry& entry) {
        if (const std::optional<math::RayHit> hit = math::Raycast::circle(ray, {entry.point, entry.radius})) {
            EntryBounds::insertHit(hits, {.id = entry.id, .point = hit->point, .normal = hit->normal, .distance = hit->distance});
        }
    };
    // clang-format on
    visitBox({.begin = 0, .end = tree.size()}, low, high, visit);
    EntryBounds::finishHits(hits, limit);
}

void KdTree::collectNearest(Range range, math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    if (range.begin >= range.end) {
        return;
    }
    const std::size_t middle = range.begin + (range.end - range.begin) / 2;
    const Entry& entry = tree[middle];
    const float distance = std::max(0.0F, math::Vec2::distance(point, entry.point) - entry.radius);
    if (distance <= maxDistance) {
        EntryBounds::keepNearest(neighbors, {.id = entry.id, .distance = distance}, count);
    }

    // The half on the side of the point goes first, and the other half only when the split plane is closer than the farthest neighbor kept.
    const float offset = along(point, range.depth) - along(entry.point, range.depth);
    const Range lower{.begin = range.begin, .end = middle, .depth = range.depth + 1};
    const Range upper{.begin = middle + 1, .end = range.end, .depth = range.depth + 1};
    collectNearest(offset <= 0.0F ? lower : upper, point, count, maxDistance, neighbors);
    const float planeDistance = std::fabs(offset) - maxRadius;
    if (planeDistance <= maxDistance && (neighbors.size() < count || planeDistance <= neighbors.back().distance)) {
        collectNearest(offset <= 0.0F ? upper : lower, point, count, maxDistance, neighbors);
    }
}

void KdTree::nearest(math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    requireBuilt();
    EntryBounds::requireRadius(maxDistance);
    neighbors.clear();
    if (count > 0) {
        collectNearest({.begin = 0, .end = tree.size()}, point, count, maxDistance, neighbors);
    }
}

} // namespace haylen::spatial2d
