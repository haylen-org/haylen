#include "haylen/2d/spatial/QuadTree.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "2d/spatial/EntryBounds.hpp"
#include "haylen/math/Raycast.hpp"

namespace haylen::spatial2d {

const QuadTree::Settings QuadTree::kDefaultSettings{};

QuadTree::QuadTree(const math::Rect& area, const Settings& value) : settings(value) {
    EntryBounds::requireValid(area);
    if (area.isEmpty() || value.maxEntries == 0 || value.maxDepth < 0) {
        throw std::invalid_argument("A quadtree needs an area of positive size, room for an entry per quadrant and a depth of zero or more.");
    }
    nodes.push_back({.bounds = area});
}

std::size_t QuadTree::getNodeCount() const noexcept {
    return nodes.size() - freeBlocks.size() * 4;
}

std::int32_t QuadTree::childFor(std::int32_t node, const math::Rect& bounds) const noexcept {
    const std::int32_t first = nodes[static_cast<std::size_t>(node)].firstChild;
    if (first < 0) {
        return -1;
    }
    for (std::int32_t child = first; child < first + 4; ++child) {
        if (nodes[static_cast<std::size_t>(child)].bounds.contains(bounds)) {
            return child;
        }
    }
    return -1;
}

void QuadTree::insert(std::uint64_t id, const math::Rect& bounds) {
    std::int32_t node = 0;
    for (std::int32_t child = childFor(node, bounds); child >= 0; child = childFor(node, bounds)) {
        node = child;
    }

    Node& target = nodes[static_cast<std::size_t>(node)];
    target.ids.push_back(id);
    entries.insert_or_assign(id, Entry{.bounds = bounds, .node = node});
    if (target.firstChild < 0 && target.ids.size() > settings.maxEntries && target.depth < settings.maxDepth) {
        split(node);
    }
}

void QuadTree::split(std::int32_t node) {
    std::int32_t first = 0;
    if (freeBlocks.empty()) {
        first = static_cast<std::int32_t>(nodes.size());
        nodes.resize(nodes.size() + 4);
    } else {
        first = freeBlocks.back();
        freeBlocks.pop_back();
    }

    const math::Rect bounds = nodes[static_cast<std::size_t>(node)].bounds;
    const math::Vec2 half = bounds.getSize() * 0.5F;
    const int depth = nodes[static_cast<std::size_t>(node)].depth + 1;
    const std::array<math::Vec2, 4> corners{bounds.getMin(), math::Vec2{bounds.x + half.x, bounds.y}, math::Vec2{bounds.x, bounds.y + half.y}, bounds.getMin() + half};
    for (std::int32_t quadrant = 0; quadrant < 4; ++quadrant) {
        Node& child = nodes[static_cast<std::size_t>(first + quadrant)];
        child.bounds = {corners[static_cast<std::size_t>(quadrant)].x, corners[static_cast<std::size_t>(quadrant)].y, half.x, half.y};
        child.parent = node;
        child.firstChild = -1;
        child.depth = depth;
        child.ids.clear();
    }
    nodes[static_cast<std::size_t>(node)].firstChild = first;

    // Entries that fit a quadrant whole move down into it, and entries that straddle quadrants stay.
    std::vector<std::uint64_t>& ids = nodes[static_cast<std::size_t>(node)].ids;
    for (std::size_t index = 0; index < ids.size();) {
        Entry& entry = entries.at(ids[index]);
        const std::int32_t child = childFor(node, entry.bounds);
        if (child < 0) {
            ++index;
            continue;
        }
        nodes[static_cast<std::size_t>(child)].ids.push_back(ids[index]);
        entry.node = child;
        ids[index] = ids.back();
        ids.pop_back();
    }

    for (std::int32_t child = first; child < first + 4; ++child) {
        if (nodes[static_cast<std::size_t>(child)].ids.size() > settings.maxEntries && depth < settings.maxDepth) {
            split(child);
        }
    }
}

std::size_t QuadTree::countBelow(std::int32_t node) const noexcept {
    const Node& current = nodes[static_cast<std::size_t>(node)];
    std::size_t count = current.ids.size();
    for (std::int32_t child = current.firstChild; current.firstChild >= 0 && child < current.firstChild + 4 && count <= settings.maxEntries; ++child) {
        count += countBelow(child);
    }
    return count;
}

void QuadTree::gather(std::int32_t node, std::vector<std::uint64_t>& ids) const {
    const Node& current = nodes[static_cast<std::size_t>(node)];
    if (current.firstChild < 0) {
        return;
    }
    for (std::int32_t child = current.firstChild; child < current.firstChild + 4; ++child) {
        const std::vector<std::uint64_t>& below = nodes[static_cast<std::size_t>(child)].ids;
        ids.insert(ids.end(), below.begin(), below.end());
        gather(child, ids);
    }
}

void QuadTree::release(std::int32_t node) {
    const std::int32_t first = nodes[static_cast<std::size_t>(node)].firstChild;
    if (first < 0) {
        return;
    }
    for (std::int32_t child = first; child < first + 4; ++child) {
        release(child);
        nodes[static_cast<std::size_t>(child)].ids.clear();
    }
    freeBlocks.push_back(first);
    nodes[static_cast<std::size_t>(node)].firstChild = -1;
}

void QuadTree::collapse(std::int32_t node) {
    for (std::int32_t current = node; current >= 0; current = nodes[static_cast<std::size_t>(current)].parent) {
        if (nodes[static_cast<std::size_t>(current)].firstChild < 0) {
            continue;
        }
        if (countBelow(current) > settings.maxEntries) {
            return;
        }

        // The whole subtree folds into this node, and its quadrant blocks become free again.
        std::vector<std::uint64_t> moved;
        gather(current, moved);
        release(current);
        for (const std::uint64_t id : moved) {
            entries.at(id).node = current;
        }
        std::vector<std::uint64_t>& ids = nodes[static_cast<std::size_t>(current)].ids;
        ids.insert(ids.end(), moved.begin(), moved.end());
    }
}

void QuadTree::set(std::uint64_t id, const math::Rect& bounds) {
    EntryBounds::requireValid(bounds);
    const auto existing = entries.find(id);
    if (existing != entries.end()) {
        // An entry stays in its quadrant while that quadrant still holds it whole and none of its children does.
        Entry& entry = existing->second;
        const bool fits = entry.node == 0 || nodes[static_cast<std::size_t>(entry.node)].bounds.contains(bounds);
        if (fits && childFor(entry.node, bounds) < 0) {
            entry.bounds = bounds;
            return;
        }
        remove(id);
    }
    insert(id, bounds);
}

bool QuadTree::remove(std::uint64_t id) {
    const auto entry = entries.find(id);
    if (entry == entries.end()) {
        return false;
    }

    const std::int32_t node = entry->second.node;
    std::vector<std::uint64_t>& ids = nodes[static_cast<std::size_t>(node)].ids;
    *std::ranges::find(ids, id) = ids.back();
    ids.pop_back();
    entries.erase(entry);
    collapse(nodes[static_cast<std::size_t>(node)].firstChild >= 0 ? node : nodes[static_cast<std::size_t>(node)].parent);
    return true;
}

void QuadTree::clear() {
    nodes.resize(1);
    nodes.front().ids.clear();
    nodes.front().firstChild = -1;
    freeBlocks.clear();
    entries.clear();
}

bool QuadTree::contains(std::uint64_t id) const {
    return entries.contains(id);
}

std::optional<math::Rect> QuadTree::getBounds(std::uint64_t id) const {
    const auto entry = entries.find(id);
    if (entry == entries.end()) {
        return std::nullopt;
    }
    return entry->second.bounds;
}

void QuadTree::collectOverlapping(std::int32_t node, const math::Rect& area, std::vector<std::uint64_t>& ids) const {
    const Node& current = nodes[static_cast<std::size_t>(node)];
    for (const std::uint64_t id : current.ids) {
        if (EntryBounds::overlaps(entries.at(id).bounds, area)) {
            ids.push_back(id);
        }
    }
    for (std::int32_t child = current.firstChild; current.firstChild >= 0 && child < current.firstChild + 4; ++child) {
        if (EntryBounds::overlaps(nodes[static_cast<std::size_t>(child)].bounds, area)) {
            collectOverlapping(child, area, ids);
        }
    }
}

void QuadTree::query(const math::Rect& area, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireValid(area);
    ids.clear();
    collectOverlapping(0, area, ids);
    std::ranges::sort(ids);
}

void QuadTree::queryCircle(math::Vec2 center, float radius, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireRadius(radius);
    query(math::Rect::fromCenter(center, {radius * 2.0F, radius * 2.0F}), ids);
    std::erase_if(ids, [&](std::uint64_t id) { return EntryBounds::distanceSquared(entries.at(id).bounds, center) > radius * radius; });
}

void QuadTree::queryPoint(math::Vec2 point, std::vector<std::uint64_t>& ids) const {
    query({point.x, point.y, 0.0F, 0.0F}, ids);
}

void QuadTree::collectCrossed(std::int32_t node, const math::Ray& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    const Node& current = nodes[static_cast<std::size_t>(node)];
    for (const std::uint64_t id : current.ids) {
        if (const std::optional<math::RayHit> hit = math::Raycast::rect(ray, entries.at(id).bounds)) {
            EntryBounds::insertHit(hits, {.id = id, .point = hit->point, .normal = hit->normal, .distance = hit->distance});
        }
    }
    for (std::int32_t child = current.firstChild; current.firstChild >= 0 && child < current.firstChild + 4; ++child) {
        const std::optional<std::array<float, 2>> travel = math::Raycast::clip(ray, nodes[static_cast<std::size_t>(child)].bounds);
        if (travel && !EntryBounds::isSettled(hits, limit, (*travel)[0])) {
            collectCrossed(child, ray, limit, hits);
        }
    }
}

void QuadTree::raycast(const math::Ray& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    hits.clear();
    collectCrossed(0, ray, limit, hits);
    EntryBounds::finishHits(hits, limit);
}

void QuadTree::collectNearest(std::int32_t node, math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    const Node& current = nodes[static_cast<std::size_t>(node)];
    for (const std::uint64_t id : current.ids) {
        const float distance = std::sqrt(EntryBounds::distanceSquared(entries.at(id).bounds, point));
        if (distance <= maxDistance) {
            EntryBounds::keepNearest(neighbors, {.id = id, .distance = distance}, count);
        }
    }
    if (current.firstChild < 0) {
        return;
    }

    // Closer quadrants go first, so the farther ones are more often skipped.
    std::array<std::pair<float, std::int32_t>, 4> children{};
    for (std::int32_t quadrant = 0; quadrant < 4; ++quadrant) {
        const std::int32_t child = current.firstChild + quadrant;
        children[static_cast<std::size_t>(quadrant)] = {std::sqrt(EntryBounds::distanceSquared(nodes[static_cast<std::size_t>(child)].bounds, point)), child};
    }
    std::ranges::sort(children);
    for (const auto& [distance, child] : children) {
        if (distance > maxDistance || (neighbors.size() == count && distance > neighbors.back().distance)) {
            return;
        }
        collectNearest(child, point, count, maxDistance, neighbors);
    }
}

void QuadTree::nearest(math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    EntryBounds::requireRadius(maxDistance);
    neighbors.clear();
    if (count > 0) {
        collectNearest(0, point, count, maxDistance, neighbors);
    }
}

} // namespace haylen::spatial2d
