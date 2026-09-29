#include "haylen/2d/spatial/AabbTree.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "2d/spatial/EntryBounds.hpp"
#include "haylen/math/Raycast.hpp"

namespace haylen::spatial2d {

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

    // Descends toward the sibling whose box grows the least, with the surface area heuristic of Box2D measured by perimeters.
    const math::Rect box = at(leaf).box;
    std::int32_t sibling = root;
    while (!at(sibling).isLeaf()) {
        const Node& node = at(sibling);
        const float combined = perimeter(node.box.merged(box));
        const float cost = 2.0F * combined;
        const float inheritance = 2.0F * (combined - perimeter(node.box));
        // clang-format off
        const auto descentCost = [&](std::int32_t child) {
            const Node& candidate = at(child);
            const float grown = perimeter(candidate.box.merged(box));
            return (candidate.isLeaf() ? grown : grown - perimeter(candidate.box)) + inheritance;
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

void AabbTree::set(std::uint64_t id, const math::Rect& bounds) {
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

std::optional<math::Rect> AabbTree::getBounds(std::uint64_t id) const {
    const auto leaf = leaves.find(id);
    if (leaf == leaves.end()) {
        return std::nullopt;
    }
    return at(leaf->second).bounds;
}

int AabbTree::getHeight() const noexcept {
    return root < 0 ? 0 : at(root).height;
}

void AabbTree::collectOverlapping(std::int32_t node, const math::Rect& area, std::vector<std::uint64_t>& ids) const {
    const Node& current = at(node);
    if (!EntryBounds::overlaps(current.box, area)) {
        return;
    }
    if (current.isLeaf()) {
        if (EntryBounds::overlaps(current.bounds, area)) {
            ids.push_back(current.id);
        }
        return;
    }
    collectOverlapping(current.left, area, ids);
    collectOverlapping(current.right, area, ids);
}

void AabbTree::query(const math::Rect& area, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireValid(area);
    ids.clear();
    if (root >= 0) {
        collectOverlapping(root, area, ids);
    }
    std::ranges::sort(ids);
}

void AabbTree::queryCircle(math::Vec2 center, float radius, std::vector<std::uint64_t>& ids) const {
    EntryBounds::requireRadius(radius);
    query(math::Rect::fromCenter(center, {radius * 2.0F, radius * 2.0F}), ids);
    std::erase_if(ids, [&](std::uint64_t id) { return EntryBounds::distanceSquared(at(leaves.at(id)).bounds, center) > radius * radius; });
}

void AabbTree::queryPoint(math::Vec2 point, std::vector<std::uint64_t>& ids) const {
    query({point.x, point.y, 0.0F, 0.0F}, ids);
}

void AabbTree::collectCrossed(std::int32_t node, const math::Ray& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    const Node& current = at(node);
    const std::optional<std::array<float, 2>> travel = math::Raycast::clip(ray, current.box);
    if (!travel || EntryBounds::isSettled(hits, limit, (*travel)[0])) {
        return;
    }
    if (current.isLeaf()) {
        if (const std::optional<math::RayHit> hit = math::Raycast::rect(ray, current.bounds)) {
            EntryBounds::insertHit(hits, {.id = current.id, .point = hit->point, .normal = hit->normal, .distance = hit->distance});
        }
        return;
    }
    collectCrossed(current.left, ray, limit, hits);
    collectCrossed(current.right, ray, limit, hits);
}

void AabbTree::raycast(const math::Ray& ray, std::size_t limit, std::vector<RayHit>& hits) const {
    hits.clear();
    if (root >= 0) {
        collectCrossed(root, ray, limit, hits);
    }
    EntryBounds::finishHits(hits, limit);
}

void AabbTree::collectNearest(std::int32_t node, math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    const Node& current = at(node);
    const float reach = std::sqrt(EntryBounds::distanceSquared(current.box, point));
    if (reach > maxDistance || (neighbors.size() == count && reach > neighbors.back().distance)) {
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

void AabbTree::nearest(math::Vec2 point, std::size_t count, float maxDistance, std::vector<Neighbor>& neighbors) const {
    EntryBounds::requireRadius(maxDistance);
    neighbors.clear();
    if (count > 0 && root >= 0) {
        collectNearest(root, point, count, maxDistance, neighbors);
    }
}

} // namespace haylen::spatial2d
