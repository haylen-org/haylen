#include "haylen/3d/spatial/AabbTree.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "3d/spatial/EntryBounds.hpp"

namespace haylen::spatial3d {

AabbTree::AabbTree(float boundsMargin) : margin(boundsMargin) {
    if (!(boundsMargin >= 0.0F) || !std::isfinite(boundsMargin)) {
        throw std::invalid_argument("An AABB tree needs a finite margin of zero or more.");
    }
}

std::int32_t AabbTree::allocate() {
    if (freeList < 0) {
        nodes.emplace_back();
        return static_cast<std::int32_t>(nodes.size() - 1);
    }

    const std::int32_t node = freeList;
    freeList = at(node).parent;
    at(node) = Node{};
    return node;
}

void AabbTree::release(std::int32_t node) noexcept {
    at(node).parent = freeList;
    at(node).height = -1;
    freeList = node;
}

void AabbTree::replaceChild(std::int32_t parent, std::int32_t from, std::int32_t to) noexcept {
    Node& owner = at(parent);
    if (owner.left == from) {
        owner.left = to;
    } else {
        owner.right = to;
    }
}

void AabbTree::insertLeaf(std::int32_t leaf) {
    if (root < 0) {
        root = leaf;
        at(leaf).parent = -1;
        return;
    }

    // Finds the sibling that adds the least box area to the tree. Pairing the leaf with a node costs the box of their new parent, and descending into a child costs what the child box grows plus what the node box grows on the way.
    const math::Box box = at(leaf).box;
    std::int32_t sibling = root;
    while (!at(sibling).isLeaf()) {
        const Node& node = at(sibling);
        const float combined = halfSurface(node.box.merged(box));
        const float cost = 2.0F * combined;
        const float inheritance = 2.0F * (combined - halfSurface(node.box));
        // clang-format off
        const auto descentCost = [&](std::int32_t child) {
            const Node& candidate = at(child);
            const float grown = halfSurface(candidate.box.merged(box));
            return (candidate.isLeaf() ? grown : grown - halfSurface(candidate.box)) + inheritance;
        };
        // clang-format on
        const float leftCost = descentCost(node.left);
        const float rightCost = descentCost(node.right);
        if (cost < leftCost && cost < rightCost) {
            break;
        }

        sibling = leftCost < rightCost ? node.left : node.right;
    }

    const std::int32_t oldParent = at(sibling).parent;
    const std::int32_t parent = allocate();
    Node& joined = at(parent);
    joined.parent = oldParent;
    joined.box = at(sibling).box.merged(box);
    joined.height = at(sibling).height + 1;
    joined.left = sibling;
    joined.right = leaf;
    at(sibling).parent = parent;
    at(leaf).parent = parent;
    if (oldParent < 0) {
        root = parent;
    } else {
        replaceChild(oldParent, sibling, parent);
    }

    refit(parent);
}

void AabbTree::removeLeaf(std::int32_t leaf) {
    if (leaf == root) {
        root = -1;
        return;
    }

    const std::int32_t parent = at(leaf).parent;
    const std::int32_t grandParent = at(parent).parent;
    const std::int32_t sibling = at(parent).left == leaf ? at(parent).right : at(parent).left;
    release(parent);
    at(sibling).parent = grandParent;

    if (grandParent < 0) {
        root = sibling;
        return;
    }

    replaceChild(grandParent, parent, sibling);
    refit(grandParent);
}

void AabbTree::refit(std::int32_t node) {
    for (std::int32_t index = node; index >= 0; index = at(index).parent) {
        index = balance(index);
        Node& current = at(index);
        current.height = 1 + std::max(at(current.left).height, at(current.right).height);
        current.box = at(current.left).box.merged(at(current.right).box);
    }
}

std::int32_t AabbTree::balance(std::int32_t a) {
    if (at(a).isLeaf() || at(a).height < 2) {
        return a;
    }

    const std::int32_t b = at(a).left;
    const std::int32_t c = at(a).right;
    const int difference = at(c).height - at(b).height;

    // The right child C rises into the place of A, keeps its taller child and hands its shorter child to A.
    if (difference > 1) {
        const std::int32_t f = at(c).left;
        const std::int32_t g = at(c).right;
        at(c).left = a;
        at(c).parent = at(a).parent;
        at(a).parent = c;
        if (at(c).parent < 0) {
            root = c;
        } else {
            replaceChild(at(c).parent, a, c);
        }

        const bool keepF = at(f).height > at(g).height;
        const std::int32_t kept = keepF ? f : g;
        const std::int32_t given = keepF ? g : f;
        at(c).right = kept;
        at(a).right = given;
        at(given).parent = a;
        at(a).box = at(b).box.merged(at(given).box);
        at(c).box = at(a).box.merged(at(kept).box);
        at(a).height = 1 + std::max(at(b).height, at(given).height);
        at(c).height = 1 + std::max(at(a).height, at(kept).height);
        return c;
    }

    // The left child B rises the same way when the left side is too tall.
    if (difference < -1) {
        const std::int32_t d = at(b).left;
        const std::int32_t e = at(b).right;
        at(b).left = a;
        at(b).parent = at(a).parent;
        at(a).parent = b;
        if (at(b).parent < 0) {
            root = b;
        } else {
            replaceChild(at(b).parent, a, b);
        }

        const bool keepD = at(d).height > at(e).height;
        const std::int32_t kept = keepD ? d : e;
        const std::int32_t given = keepD ? e : d;
        at(b).right = kept;
        at(a).left = given;
        at(given).parent = a;
        at(a).box = at(c).box.merged(at(given).box);
        at(b).box = at(a).box.merged(at(kept).box);
        at(a).height = 1 + std::max(at(c).height, at(given).height);
        at(b).height = 1 + std::max(at(a).height, at(kept).height);
        return b;
    }

    return a;
}

void AabbTree::set(std::uint64_t id, const math::Box& bounds) {
    EntryBounds::requireValid(bounds);
    const auto existing = leaves.find(id);
    if (existing != leaves.end()) {
        const std::int32_t leaf = existing->second;
        at(leaf).bounds = bounds;
        if (at(leaf).box.contains(bounds)) {
            return;
        }

        removeLeaf(leaf);
        at(leaf).box = bounds.expanded(margin);
        insertLeaf(leaf);
        return;
    }

    const std::int32_t leaf = allocate();
    Node& node = at(leaf);
    node.box = bounds.expanded(margin);
    node.bounds = bounds;
    node.id = id;
    insertLeaf(leaf);
    leaves.emplace(id, leaf);
}

void AabbTree::setAll(std::span<const std::uint64_t> ids, std::span<const math::Box> bounds) {
    EntryBounds::requireBatch(ids.size(), bounds.size());
    for (const math::Box& box : bounds) {
        EntryBounds::requireValid(box);
    }

    for (std::size_t index = 0; index < ids.size(); ++index) {
        set(ids[index], bounds[index]);
    }
}

bool AabbTree::remove(std::uint64_t id) {
    const auto leaf = leaves.find(id);
    if (leaf == leaves.end()) {
        return false;
    }

    removeLeaf(leaf->second);
    release(leaf->second);
    leaves.erase(leaf);
    return true;
}

void AabbTree::clear() noexcept {
    nodes.clear();
    root = -1;
    freeList = -1;
    leaves.clear();
}

bool AabbTree::contains(std::uint64_t id) const {
    return leaves.contains(id);
}

std::optional<math::Box> AabbTree::getBounds(std::uint64_t id) const {
    const auto leaf = leaves.find(id);
    if (leaf == leaves.end()) {
        return std::nullopt;
    }

    return at(leaf->second).bounds;
}

int AabbTree::getHeight() const noexcept {
    return root < 0 ? 0 : at(root).height;
}

template <typename Reaches, typename Accepts> void AabbTree::collect(std::int32_t node, const Reaches& reaches, const Accepts& accepts, std::vector<std::uint64_t>& ids) const {
    const Node& current = at(node);
    if (!reaches(current.box)) {
        return;
    }

    if (current.isLeaf()) {
        if (accepts(current.bounds)) {
            ids.push_back(current.id);
        }

        return;
    }

    collect(current.left, reaches, accepts, ids);
    collect(current.right, reaches, accepts, ids);
}

void AabbTree::query(const math::Box& area, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireValid(area);
    ids.clear();
    const auto overlaps = [&area](const math::Box& box) { return box.intersects(area); };
    if (root >= 0) {
        collect(root, overlaps, overlaps, ids);
    }

    std::ranges::sort(ids);
}

void AabbTree::querySphere(const math::Sphere& sphere, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireSphere(sphere);
    ids.clear();
    const auto touches = [&sphere](const math::Box& box) { return EntryBounds::touches(box, sphere); };
    if (root >= 0) {
        collect(root, touches, touches, ids);
    }

    std::ranges::sort(ids);
}

void AabbTree::queryPoint(math::Vec3 point, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requirePoint(point);
    query({point, point}, ids);
}

void AabbTree::collectAll(std::int32_t node, std::vector<std::uint64_t>& ids) const {
    const Node& current = at(node);
    if (current.isLeaf()) {
        ids.push_back(current.id);
        return;
    }

    collectAll(current.left, ids);
    collectAll(current.right, ids);
}

// A branch whose box lies wholly inside the frustum holds only entries inside it, so they join without a test.
void AabbTree::collectVisible(std::int32_t node, const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const {
    const Node& current = at(node);
    const math::Frustum::Containment containment = frustum.classify(current.box);
    if (containment == math::Frustum::Containment::Outside) {
        return;
    }

    if (containment == math::Frustum::Containment::Inside) {
        collectAll(node, ids);
        return;
    }

    if (current.isLeaf()) {
        if (frustum.classify(current.bounds) != math::Frustum::Containment::Outside) {
            ids.push_back(current.id);
        }

        return;
    }

    collectVisible(current.left, frustum, ids);
    collectVisible(current.right, frustum, ids);
}

void AabbTree::queryFrustum(const math::Frustum& frustum, std::vector<std::uint64_t>& ids) const {
    ids.clear();
    if (root >= 0) {
        collectVisible(root, frustum, ids);
    }

    std::ranges::sort(ids);
}

void AabbTree::collectCrossed(std::int32_t node, float entry, const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    if (EntryBounds::isSettled(hits, limit, entry)) {
        return;
    }

    const Node& current = at(node);
    if (current.isLeaf()) {
        EntryBounds::addHit(hits, current.id, ray, current.bounds);
        return;
    }

    const std::optional<float> leftEntry = EntryBounds::enter(ray, at(current.left).box);
    const std::optional<float> rightEntry = EntryBounds::enter(ray, at(current.right).box);
    const bool rightFirst = rightEntry && (!leftEntry || *rightEntry < *leftEntry);
    const std::optional<float> soonerEntry = rightFirst ? rightEntry : leftEntry;
    const std::optional<float> laterEntry = rightFirst ? leftEntry : rightEntry;
    if (soonerEntry) {
        collectCrossed(rightFirst ? current.right : current.left, *soonerEntry, ray, limit, hits);
    }

    if (laterEntry) {
        collectCrossed(rightFirst ? current.left : current.right, *laterEntry, ray, limit, hits);
    }
}

void AabbTree::raycast(const math::Ray3D& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    hits.clear();
    const std::optional<float> entry = root >= 0 ? EntryBounds::enter(ray, at(root).box) : std::nullopt;
    if (entry) {
        collectCrossed(root, *entry, ray, limit, hits);
    }

    EntryBounds::finishHits(hits, limit);
}

void AabbTree::collectNearest(std::int32_t node, math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    const Node& current = at(node);
    const float reach = std::sqrt(EntryBounds::distanceSquared(current.box, point));
    if (EntryBounds::isBeyond(neighbors, count, reach, maxDistance)) {
        return;
    }

    if (current.isLeaf()) {
        const float distance = std::sqrt(EntryBounds::distanceSquared(current.bounds, point));
        if (distance <= maxDistance) {
            EntryBounds::keepNearest(neighbors, {.id = current.id, .distance = distance}, count);
        }

        return;
    }

    const bool leftFirst = EntryBounds::distanceSquared(at(current.left).box, point) <= EntryBounds::distanceSquared(at(current.right).box, point);
    collectNearest(leftFirst ? current.left : current.right, point, count, maxDistance, neighbors);
    collectNearest(leftFirst ? current.right : current.left, point, count, maxDistance, neighbors);
}

void AabbTree::nearest(math::Vec3 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    EntryBounds::requirePoint(point);
    EntryBounds::requireRadius(maxDistance);
    neighbors.clear();
    if (count > 0 && root >= 0) {
        collectNearest(root, point, count, maxDistance, neighbors);
    }

    EntryBounds::finishNearest(neighbors);
}

} // namespace haylen::spatial3d
