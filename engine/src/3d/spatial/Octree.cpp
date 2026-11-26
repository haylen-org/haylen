#include "haylen/3d/spatial/Octree.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "3d/spatial/EntryBounds.hpp"

namespace haylen::spatial3d {

const Octree::Settings Octree::kDefaultSettings{};

Octree::Octree(const math::Box& region, const Settings& value) : settings(value) {
    EntryBounds::requireValid(region);
    const math::Vec3 size = region.getSize();
    const bool solid = size.x > 0.0F && size.y > 0.0F && size.z > 0.0F && size.isFinite();
    if (!solid || value.maxEntries == 0 || value.maxDepth < 0 || value.maxDepth > kMaxDepth || !(value.looseness >= 0.0F && value.looseness <= kMaxLooseness)) {
        throw std::invalid_argument("An octree needs a box with volume, room for an entry per node, a depth from 0 to 16 and a looseness from 0 to 4.");
    }

    const math::Vec3 grow = size * (0.5F * value.looseness);
    nodes.push_back({.cell = region, .reach = {region.min - grow, region.max + grow}});
}

std::size_t Octree::getNodeCount() const noexcept {
    return nodes.size() - freeBlocks.size() * kChildren;
}

std::int32_t Octree::childFor(std::int32_t node, const math::Box& bounds) const noexcept {
    const Node& current = at(node);
    if (current.firstChild < 0) {
        return -1;
    }

    const math::Vec3 middle = current.cell.getCenter();
    const math::Vec3 center = bounds.min * 0.5F + bounds.max * 0.5F;
    const std::int32_t octant = (center.x >= middle.x ? 1 : 0) | (center.y >= middle.y ? 2 : 0) | (center.z >= middle.z ? 4 : 0);
    const std::int32_t child = current.firstChild + octant;
    return at(child).reach.contains(bounds) ? child : -1;
}

void Octree::insert(std::uint64_t id, const math::Box& bounds) {
    std::int32_t node = 0;
    for (std::int32_t child = childFor(node, bounds); child >= 0; child = childFor(node, bounds)) {
        node = child;
    }

    Node& target = at(node);
    target.ids.push_back(id);
    entries.insert_or_assign(id, Entry{.bounds = bounds, .node = node});

    if (target.firstChild < 0 && target.ids.size() > settings.maxEntries && target.depth < settings.maxDepth) {
        split(node);
    }
}

void Octree::split(std::int32_t node) {
    std::int32_t first = 0;
    if (freeBlocks.empty()) {
        first = static_cast<std::int32_t>(nodes.size());
        nodes.resize(nodes.size() + kChildren);
    } else {
        first = freeBlocks.back();
        freeBlocks.pop_back();
    }

    const math::Box cell = at(node).cell;
    const math::Vec3 half = cell.getSize() * 0.5F;
    const math::Vec3 grow = half * (0.5F * settings.looseness);
    const int depth = at(node).depth + 1;
    for (std::int32_t octant = 0; octant < kChildren; ++octant) {
        const math::Vec3 low{(octant & 1) != 0 ? cell.min.x + half.x : cell.min.x, (octant & 2) != 0 ? cell.min.y + half.y : cell.min.y, (octant & 4) != 0 ? cell.min.z + half.z : cell.min.z};
        Node& child = at(first + octant);
        child.cell = {low, low + half};
        child.reach = {low - grow, low + half + grow};
        child.parent = node;
        child.firstChild = -1;
        child.depth = depth;
        child.ids.clear();
    }

    at(node).firstChild = first;

    // Entries whose octant holds them whole move down into it, and the others stay.
    std::vector<std::uint64_t>& ids = at(node).ids;
    for (std::size_t index = 0; index < ids.size();) {
        Entry& entry = entries.at(ids[index]);
        const std::int32_t child = childFor(node, entry.bounds);
        if (child < 0) {
            ++index;
            continue;
        }

        at(child).ids.push_back(ids[index]);
        entry.node = child;
        ids[index] = ids.back();
        ids.pop_back();
    }

    for (std::int32_t child = first; child < first + kChildren; ++child) {
        if (at(child).ids.size() > settings.maxEntries && depth < settings.maxDepth) {
            split(child);
        }
    }
}

std::size_t Octree::countBelow(std::int32_t node) const noexcept {
    const Node& current = at(node);
    std::size_t count = current.ids.size();
    for (std::int32_t child = current.firstChild; current.firstChild >= 0 && child < current.firstChild + kChildren && count <= settings.maxEntries; ++child) {
        count += countBelow(child);
    }

    return count;
}

void Octree::gather(std::int32_t node, std::vector<std::uint64_t>& ids) const {
    const Node& current = at(node);
    if (current.firstChild < 0) {
        return;
    }

    for (std::int32_t child = current.firstChild; child < current.firstChild + kChildren; ++child) {
        const std::vector<std::uint64_t>& below = at(child).ids;
        ids.insert(ids.end(), below.begin(), below.end());
        gather(child, ids);
    }
}

void Octree::release(std::int32_t node) {
    const std::int32_t first = at(node).firstChild;
    if (first < 0) {
        return;
    }

    for (std::int32_t child = first; child < first + kChildren; ++child) {
        release(child);
        at(child).ids.clear();
    }

    freeBlocks.push_back(first);
    at(node).firstChild = -1;
}

void Octree::collapse(std::int32_t node) {
    for (std::int32_t current = node; current >= 0; current = at(current).parent) {
        if (at(current).firstChild < 0) {
            continue;
        }

        if (countBelow(current) > settings.maxEntries) {
            return;
        }

        // The whole subtree folds into this node, and its blocks of children become free again.
        std::vector<std::uint64_t> moved;
        gather(current, moved);
        release(current);

        for (const std::uint64_t id : moved) {
            entries.at(id).node = current;
        }

        std::vector<std::uint64_t>& ids = at(current).ids;
        ids.insert(ids.end(), moved.begin(), moved.end());
    }
}

void Octree::set(std::uint64_t id, const math::Box& bounds) {
    EntryBounds::requireValid(bounds);
    const auto existing = entries.find(id);
    if (existing != entries.end()) {
        // An entry stays in its node while the reach of that node still holds it whole and no child takes it.
        Entry& entry = existing->second;
        const bool fits = entry.node == 0 || at(entry.node).reach.contains(bounds);
        if (fits && childFor(entry.node, bounds) < 0) {
            entry.bounds = bounds;
            return;
        }

        remove(id);
    }

    insert(id, bounds);
}

void Octree::setAll(std::span<const std::uint64_t> ids, std::span<const math::Box> bounds) {
    EntryBounds::requireBatch(ids.size(), bounds.size());
    for (const math::Box& box : bounds) {
        EntryBounds::requireValid(box);
    }

    for (std::size_t index = 0; index < ids.size(); ++index) {
        set(ids[index], bounds[index]);
    }
}

bool Octree::remove(std::uint64_t id) {
    const auto entry = entries.find(id);
    if (entry == entries.end()) {
        return false;
    }

    const std::int32_t node = entry->second.node;
    std::vector<std::uint64_t>& ids = at(node).ids;
    *std::ranges::find(ids, id) = ids.back();
    ids.pop_back();
    entries.erase(entry);
    collapse(at(node).firstChild >= 0 ? node : at(node).parent);
    return true;
}

void Octree::clear() {
    nodes.resize(1);
    nodes.front().ids.clear();
    nodes.front().firstChild = -1;
    freeBlocks.clear();
    entries.clear();
}

bool Octree::contains(std::uint64_t id) const {
    return entries.contains(id);
}

std::optional<math::Box> Octree::getBounds(std::uint64_t id) const {
    const auto entry = entries.find(id);
    if (entry == entries.end()) {
        return std::nullopt;
    }

    return entry->second.bounds;
}

// The root also holds the entries outside the region, so only the children are skipped by their reach.
template <typename Reaches, typename Accepts> void Octree::collect(std::int32_t node, const Reaches& reaches, const Accepts& accepts, std::vector<std::uint64_t>& ids) const {
    const Node& current = at(node);
    for (const std::uint64_t id : current.ids) {
        if (accepts(entries.at(id).bounds)) {
            ids.push_back(id);
        }
    }

    for (std::int32_t child = current.firstChild; current.firstChild >= 0 && child < current.firstChild + kChildren; ++child) {
        if (reaches(at(child).reach)) {
            collect(child, reaches, accepts, ids);
        }
    }
}

void Octree::query(const math::Box& area, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireValid(area);
    ids.clear();
    const auto overlaps = [&area](const math::Box& box) { return box.intersects(area); };
    collect(0, overlaps, overlaps, ids);
    std::ranges::sort(ids);
}

void Octree::querySphere(const math::Sphere& sphere, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireSphere(sphere);
    ids.clear();
    const auto touches = [&sphere](const math::Box& box) { return EntryBounds::touches(box, sphere); };
    collect(0, touches, touches, ids);
    std::ranges::sort(ids);
}

void Octree::queryPoint(math::Vec3 point, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requirePoint(point);
    query({point, point}, ids);
}

void Octree::collectAll(std::int32_t node, std::vector<std::uint64_t>& ids) const {
    const Node& current = at(node);
    ids.insert(ids.end(), current.ids.begin(), current.ids.end());
    for (std::int32_t child = current.firstChild; current.firstChild >= 0 && child < current.firstChild + kChildren; ++child) {
        collectAll(child, ids);
    }
}

// A child whose reach lies wholly inside the frustum holds only entries inside it, so they join without a test.
void Octree::collectVisible(std::int32_t node, const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const {
    const Node& current = at(node);
    for (const std::uint64_t id : current.ids) {
        if (frustum.classify(entries.at(id).bounds) != math::Frustum::Containment::Outside) {
            ids.push_back(id);
        }
    }

    for (std::int32_t child = current.firstChild; current.firstChild >= 0 && child < current.firstChild + kChildren; ++child) {
        const math::Frustum::Containment containment = frustum.classify(at(child).reach);
        if (containment == math::Frustum::Containment::Inside) {
            collectAll(child, ids);
        } else if (containment == math::Frustum::Containment::Intersecting) {
            collectVisible(child, frustum, ids);
        }
    }
}

void Octree::queryFrustum(const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const {
    ids.clear();
    collectVisible(0, frustum, ids);
    std::ranges::sort(ids);
}

void Octree::collectCrossed(std::int32_t node, const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    const Node& current = at(node);
    for (const std::uint64_t id : current.ids) {
        EntryBounds::addHit(hits, id, ray, entries.at(id).bounds);
    }

    if (current.firstChild < 0) {
        return;
    }

    // The children go in the order the ray enters their reach, so a hit in a near child often settles the far ones.
    std::array<std::pair<float, std::int32_t>, kChildren> children{};
    std::size_t crossed = 0;
    for (std::int32_t child = current.firstChild; child < current.firstChild + kChildren; ++child) {
        if (const std::optional<float> entry = EntryBounds::enter(ray, at(child).reach)) {
            children[crossed++] = {*entry, child};
        }
    }

    std::sort(children.begin(), children.begin() + static_cast<std::ptrdiff_t>(crossed));
    for (std::size_t index = 0; index < crossed; ++index) {
        if (!EntryBounds::isSettled(hits, limit, children[index].first)) {
            collectCrossed(children[index].second, ray, limit, hits);
        }
    }
}

void Octree::raycast(const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    hits.clear();
    collectCrossed(0, ray, limit, hits);
    EntryBounds::finishHits(hits, limit);
}

void Octree::collectNearest(std::int32_t node, math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    const Node& current = at(node);
    for (const std::uint64_t id : current.ids) {
        const float distance = std::sqrt(EntryBounds::distanceSquared(entries.at(id).bounds, point));
        if (distance <= maxDistance) {
            EntryBounds::keepNearest(neighbors, {.id = id, .distance = distance}, count);
        }
    }

    if (current.firstChild < 0) {
        return;
    }

    // Closer children go first, so the farther ones are more often skipped.
    std::array<std::pair<float, std::int32_t>, kChildren> children{};
    for (std::int32_t octant = 0; octant < kChildren; ++octant) {
        const std::int32_t child = current.firstChild + octant;
        children[static_cast<std::size_t>(octant)] = {std::sqrt(EntryBounds::distanceSquared(at(child).reach, point)), child};
    }

    std::ranges::sort(children);

    for (const auto& [distance, child] : children) {
        if (EntryBounds::isBeyond(neighbors, count, distance, maxDistance)) {
            return;
        }

        collectNearest(child, point, count, maxDistance, neighbors);
    }
}

void Octree::nearest(math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    EntryBounds::requirePoint(point);
    EntryBounds::requireRadius(maxDistance);
    neighbors.clear();
    if (count > 0) {
        collectNearest(0, point, count, maxDistance, neighbors);
    }

    EntryBounds::finishNearest(neighbors);
}

} // namespace haylen::spatial3d
