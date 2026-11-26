#include "haylen/3d/spatial/Bvh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "3d/spatial/EntryBounds.hpp"

namespace haylen::spatial3d {

void Bvh::set(std::uint64_t id, const math::Box& bounds) {
    EntryBounds::requireValid(bounds);
    const auto existing = positions.find(id);
    if (existing != positions.end()) {
        entries[existing->second].bounds = bounds;
        state = state == State::Built ? State::Moved : state;
        return;
    }

    if (entries.size() == std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("A BVH holds at most 4294967295 entries.");
    }

    positions.emplace(id, static_cast<std::uint32_t>(entries.size()));
    entries.push_back({.id = id, .bounds = bounds});
    state = State::Changed;
}

void Bvh::setAll(std::span<const std::uint64_t> ids, std::span<const math::Box> bounds) {
    EntryBounds::requireBatch(ids.size(), bounds.size());
    for (const math::Box& box : bounds) {
        EntryBounds::requireValid(box);
    }

    for (std::size_t index = 0; index < ids.size(); ++index) {
        set(ids[index], bounds[index]);
    }
}

bool Bvh::remove(std::uint64_t id) {
    const auto existing = positions.find(id);
    if (existing == positions.end()) {
        return false;
    }

    const std::uint32_t position = existing->second;
    positions.erase(existing);
    if (position + 1 != entries.size()) {
        entries[position] = entries.back();
        positions[entries[position].id] = position;
    }

    entries.pop_back();
    state = State::Changed;
    return true;
}

void Bvh::clear() noexcept {
    entries.clear();
    positions.clear();
    tree.clear();
    state = State::Built;
}

void Bvh::build() {
    std::vector<math::Box> boxes;
    boxes.reserve(entries.size());
    for (const Entry& entry : entries) {
        boxes.push_back(entry.bounds);
    }

    std::vector<std::uint32_t> order;
    tree.build(boxes, order);

    // The entries take the order of the leaves, so every leaf reads a run of them.
    std::vector<Entry> ordered;
    ordered.reserve(entries.size());
    for (const std::uint32_t position : order) {
        ordered.push_back(entries[position]);
    }

    entries = std::move(ordered);
    for (std::uint32_t position = 0; position < entries.size(); ++position) {
        positions[entries[position].id] = position;
    }

    state = State::Built;
}

void Bvh::refit() {
    if (state == State::Changed) {
        throw std::logic_error("Entries came or went since the last build of the BVH, so it needs \"build\" instead of \"refit\".");
    }

    // clang-format off
    tree.refit([this](std::uint32_t first, std::uint32_t count) {
        math::Box box = entries[first].bounds;
        for (std::uint32_t entry = first + 1; entry < first + count; ++entry) {
            box = box.merged(entries[entry].bounds);
        }

        return box;
    });
    // clang-format on
    state = State::Built;
}

void Bvh::requireBuilt() const {
    if (state != State::Built) {
        throw std::logic_error("The BVH changed since its last build. Call \"build\", or \"refit\" when its entries only moved, before a query.");
    }
}

bool Bvh::contains(std::uint64_t id) const {
    return positions.contains(id);
}

std::optional<math::Box> Bvh::getBounds(std::uint64_t id) const {
    const auto existing = positions.find(id);
    if (existing == positions.end()) {
        return std::nullopt;
    }

    return entries[existing->second].bounds;
}

template <typename Accepts> void Bvh::keep(std::uint32_t first, std::uint32_t count, const Accepts& accepts, std::vector<std::uint64_t>& ids) const {
    for (std::uint32_t entry = first; entry < first + count; ++entry) {
        if (accepts(entries[entry].bounds)) {
            ids.push_back(entries[entry].id);
        }
    }
}

void Bvh::query(const math::Box& area, std::vector<std::uint64_t>& ids) const {
    requireBuilt();
    EntryBounds::requireValid(area);
    ids.clear();
    const auto overlaps = [&area](const math::Box& box) { return box.intersects(area); };
    tree.visit(overlaps, [&](std::uint32_t first, std::uint32_t count) { keep(first, count, overlaps, ids); });
    std::ranges::sort(ids);
}

void Bvh::querySphere(const math::Sphere& sphere, std::vector<std::uint64_t>& ids) const {
    requireBuilt();
    EntryBounds::requireSphere(sphere);
    ids.clear();
    const auto touches = [&sphere](const math::Box& box) { return EntryBounds::touches(box, sphere); };
    tree.visit(touches, [&](std::uint32_t first, std::uint32_t count) { keep(first, count, touches, ids); });
    std::ranges::sort(ids);
}

void Bvh::queryPoint(math::Vec3 point, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requirePoint(point);
    query({point, point}, ids);
}

// A node whose box lies wholly inside the frustum holds only entries inside it, and they lie next to each other, so they join without a test.
void Bvh::queryFrustum(const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const {
    requireBuilt();
    ids.clear();
    const auto all = [](const math::Box&) { return true; };
    const auto visible = [&frustum](const math::Box& box) { return frustum.classify(box) != math::Frustum::Containment::Outside; };
    tree.visitFrustum(frustum, [&](std::uint32_t first, std::uint32_t count) { keep(first, count, all, ids); }, [&](std::uint32_t first, std::uint32_t count) { keep(first, count, visible, ids); });
    std::ranges::sort(ids);
}

void Bvh::raycast(const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    requireBuilt();
    hits.clear();
    const auto settled = [&](float distance) { return EntryBounds::isSettled(hits, limit, distance); };
    // clang-format off
    tree.visitRay(ray, settled, [&](std::uint32_t first, std::uint32_t count) {
        for (std::uint32_t entry = first; entry < first + count; ++entry) {
            EntryBounds::addHit(hits, entries[entry].id, ray, entries[entry].bounds);
        }
    });
    // clang-format on
    EntryBounds::finishHits(hits, limit);
}

void Bvh::nearest(math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    requireBuilt();
    EntryBounds::requirePoint(point);
    EntryBounds::requireRadius(maxDistance);
    neighbors.clear();
    if (count == 0) {
        return;
    }

    const auto beyond = [&](float distance) { return EntryBounds::isBeyond(neighbors, count, distance, maxDistance); };
    // clang-format off
    tree.visitNearest(point, beyond, [&](std::uint32_t first, std::uint32_t leafCount) {
        for (std::uint32_t entry = first; entry < first + leafCount; ++entry) {
            const float distance = std::sqrt(EntryBounds::distanceSquared(entries[entry].bounds, point));
            if (distance <= maxDistance) {
                EntryBounds::keepNearest(neighbors, {.id = entries[entry].id, .distance = distance}, count);
            }
        }
    });
    // clang-format on
    EntryBounds::finishNearest(neighbors);
}

} // namespace haylen::spatial3d
