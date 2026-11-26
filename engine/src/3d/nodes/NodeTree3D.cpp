#include "haylen/3d/nodes/NodeTree3D.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/core/JobSystem.hpp"

namespace haylen::nodes3d {

std::shared_ptr<NodeTree3D> NodeTree3D::create() {
    return std::make_shared<NodeTree3D>(Passkey{});
}

Node3D NodeTree3D::add(const NodeProperties3D& properties) {
    return insert(kNone, properties);
}

Node3D NodeTree3D::add(const Node3D& parent, const NodeProperties3D& properties) {
    const std::uint32_t index = check(parent);
    const Node3D node = insert(static_cast<std::int32_t>(index), properties);
    notify(parent.getKey(), NodeEvent3D::ChildAdded, node);
    return node;
}

Node3D NodeTree3D::insert(std::int32_t parent, const NodeProperties3D& properties) {
    if (count >= kMaxNodes) {
        throw std::length_error("A 3D node tree holds at most 16777216 nodes.");
    }

    const math::Quat rotation = properties.rotation.getNormalized();

    const std::int32_t index = allocate();
    Slot& slot = slots[static_cast<std::size_t>(index)];
    slot.position = properties.position;
    slot.rotation = rotation;
    slot.scale = properties.scale;
    slot.layers = properties.layers;
    slot.renderOrder = properties.renderOrder;
    slot.inheritance = properties.inheritance;
    slot.visible = properties.visible;
    slot.enabled = properties.enabled;
    slot.isStatic = properties.isStatic;
    slot.frustumCulling = properties.frustumCulling;

    Detail& detail = details[static_cast<std::size_t>(index)];
    detail.name = properties.name;
    detail.data = properties.data;
    link(index, parent);
    ++count;
    orderChanged = true;
    touch(static_cast<std::uint32_t>(index));

    for (const std::string& tag : properties.tags) {
        addTag(static_cast<std::uint32_t>(index), tag);
    }

    return handle(index);
}

// The copy reads the whole subtree before it adds a node, so a parent inside the subtree never takes part of its own copy, and no listener runs until the copy is complete.
Node3D NodeTree3D::clone(const Node3D& source, const Node3D& parent) {
    const std::uint32_t root = check(source);
    const std::int32_t target = parent == Node3D{} ? kNone : static_cast<std::int32_t>(check(parent));

    std::vector<Copy> copies;
    std::vector<std::pair<std::uint32_t, std::int32_t>> stack{{root, kNone}};
    while (!stack.empty()) {
        const auto [index, copiedParent] = stack.back();
        stack.pop_back();

        const Slot& slot = slots[index];
        const Detail& detail = details[index];
        NodeProperties3D properties{.name = detail.name, .position = slot.position, .rotation = slot.rotation, .scale = slot.scale, .visible = slot.visible, .enabled = slot.enabled, .layers = slot.layers, .isStatic = slot.isStatic, .frustumCulling = slot.frustumCulling, .renderOrder = slot.renderOrder, .inheritance = slot.inheritance, .data = detail.data};
        for (const auto& [tag, place] : detail.tags) {
            properties.tags.push_back(tagNames[tag]);
        }

        const auto position = static_cast<std::int32_t>(copies.size());
        copies.push_back({.source = index, .parent = copiedParent, .properties = std::move(properties)});
        for (std::int32_t child = detail.lastChild; child != kNone; child = details[static_cast<std::size_t>(child)].previous) {
            stack.emplace_back(static_cast<std::uint32_t>(child), position);
        }
    }

    std::vector<std::pair<Node3D, Node3D>> made;
    made.reserve(copies.size());
    for (const Copy& copy : copies) {
        const std::int32_t under = copy.parent == kNone ? target : static_cast<std::int32_t>(made[static_cast<std::size_t>(copy.parent)].second.index);
        made.emplace_back(handle(static_cast<std::int32_t>(copy.source)), insert(under, copy.properties));
    }

    const Node3D result = made.front().second;
    if (target != kNone) {
        notify(parent.getKey(), NodeEvent3D::ChildAdded, result);
    }

    for (const auto& [original, copied] : made) {
        cloned.emit(original, copied);
    }

    return result;
}

Node3D NodeTree3D::find(std::string_view path) const {
    const std::int32_t found = findPath(kNone, path);
    return found == kNone ? Node3D{} : handle(found);
}

std::vector<Node3D> NodeTree3D::getRoots() const {
    std::vector<Node3D> roots;
    for (std::int32_t index = firstRoot; index != kNone; index = details[static_cast<std::size_t>(index)].next) {
        roots.push_back(handle(index));
    }

    return roots;
}

std::vector<Node3D> NodeTree3D::findTagged(std::string_view tag) const {
    const auto found = tagIds.find(tag);
    if (found == tagIds.end()) {
        return {};
    }

    std::vector<Node3D> nodes;
    nodes.reserve(tagMembers[found->second].size());
    for (const std::uint32_t index : tagMembers[found->second]) {
        nodes.push_back(handle(static_cast<std::int32_t>(index)));
    }

    return nodes;
}

Node3D NodeTree3D::findByKey(std::uint64_t key) const {
    const auto slot = static_cast<std::uint32_t>(key & 0xFFFFFFFFU);
    const auto generation = static_cast<std::uint32_t>(key >> 32U);
    if (slot >= slots.size() || !holds(slot, generation)) {
        return {};
    }

    return handle(static_cast<std::int32_t>(slot));
}

void NodeTree3D::clear() {
    while (firstRoot != kNone) {
        release(firstRoot);
    }
}

void NodeTree3D::update(core::JobSystem* jobs) {
    refresh(jobs);
}

void NodeTree3D::refresh(core::JobSystem* jobs) {
    if (orderChanged) {
        rebuildOrder();
    }

    if (dirty.empty()) {
        return;
    }

    collectRanges();
    updated = 0;
    for (const Range& range : ranges) {
        updated += range.end - range.begin;
    }

    if (jobs == nullptr || updated < kParallelNodes) {
        for (const Range& range : ranges) {
            updateRange(range.begin, range.end);
        }

        return;
    }

    splitRanges();
    // clang-format off
    jobs->parallelFor(0, chunks.size(), 1, [this](std::size_t begin, std::size_t end) {
        for (std::size_t chunk = begin; chunk < end; ++chunk) {
            for (std::size_t piece = chunks[chunk].begin; piece < chunks[chunk].end; ++piece) {
                updateRange(pieces[piece].begin, pieces[piece].end);
            }
        }
    });
    // clang-format on
}

// A few changes among many nodes sort their places, and many changes scan the order, which finds the same subtrees.
void NodeTree3D::collectRanges() {
    ranges.clear();
    if (dirty.size() * kScanShare >= order.size()) {
        std::size_t place = 0;
        while (place < order.size()) {
            if (!slots[order[place]].changed) {
                ++place;
                continue;
            }

            ranges.push_back({.begin = place, .end = subtreeEnds[place]});
            place = subtreeEnds[place];
        }
    } else {
        dirtyPlaces.clear();
        for (const std::uint32_t index : dirty) {
            if (slots[index].alive && slots[index].changed) {
                dirtyPlaces.push_back(places[index]);
            }
        }

        std::ranges::sort(dirtyPlaces);
        std::size_t covered = 0;
        for (const std::uint32_t place : dirtyPlaces) {
            if (place < covered) {
                continue;
            }

            covered = subtreeEnds[place];
            ranges.push_back({.begin = place, .end = covered});
        }
    }

    dirty.clear();
}

// A range larger than the grain updates its root here and goes on as the subtrees of its children, which need nothing but that root, so even one deep subtree spreads over the workers.
void NodeTree3D::splitRanges() {
    pieces.clear();
    chunks.clear();
    for (std::size_t next = 0; next < ranges.size(); ++next) {
        const Range range = ranges[next];
        if (range.end - range.begin <= kParallelGrain) {
            pieces.push_back(range);
            continue;
        }

        updateNode(order[range.begin]);
        for (std::size_t child = range.begin + 1; child < range.end; child = subtreeEnds[child]) {
            ranges.push_back({.begin = child, .end = subtreeEnds[child]});
        }
    }

    std::size_t first = 0;
    std::size_t nodes = 0;
    for (std::size_t piece = 0; piece < pieces.size(); ++piece) {
        nodes += pieces[piece].end - pieces[piece].begin;
        if (nodes >= kParallelGrain || piece + 1 == pieces.size()) {
            chunks.push_back({.begin = first, .end = piece + 1});
            first = piece + 1;
            nodes = 0;
        }
    }
}

void NodeTree3D::updateRange(std::size_t begin, std::size_t end) noexcept {
    for (std::size_t place = begin; place < end; ++place) {
        updateNode(order[place]);
    }
}

// A node that takes every part of its parent multiplies the matrices, which keeps the shear of a parent scaled differently on each axis. One that leaves a part out builds its matrix from the parts it takes.
void NodeTree3D::updateNode(std::uint32_t index) noexcept {
    Slot& slot = slots[index];
    slot.changed = false;
    if (slot.parent == kNone) {
        slot.world = math::Mat4::compose(slot.position, slot.rotation, slot.scale);
        slot.worldRotation = slot.rotation;
        slot.worldScale = slot.scale;
        slot.worldEnabled = slot.enabled;
        slot.worldVisible = slot.visible && slot.enabled;
        return;
    }

    const Slot& parent = slots[static_cast<std::size_t>(slot.parent)];
    slot.worldEnabled = slot.enabled && parent.worldEnabled;
    slot.worldVisible = slot.visible && slot.worldEnabled && parent.worldVisible;
    slot.worldRotation = slot.inheritance.rotation ? parent.worldRotation * slot.rotation : slot.rotation;
    slot.worldScale = slot.inheritance.scale ? parent.worldScale * slot.scale : slot.scale;
    if (slot.inheritance.isFull()) {
        slot.world = parent.world * math::Mat4::compose(slot.position, slot.rotation, slot.scale);
        return;
    }

    const math::Vec3 position = slot.inheritance.position ? parent.world.apply(slot.position) : slot.position;
    slot.world = math::Mat4::compose(position, slot.worldRotation, slot.worldScale);
}

void NodeTree3D::writeTransforms(std::span<const Node3D> nodes, std::span<const float> values) {
    if (values.size() < nodes.size() * kTransformFloats) {
        throw std::invalid_argument("Node3D transforms take seven floats for each node.");
    }

    for (std::size_t place = 0; place < nodes.size(); ++place) {
        (void)check(nodes[place]);
        const std::span<const float> node = values.subspan(place * kTransformFloats, kTransformFloats);
        (void)math::Quat{node[3], node[4], node[5], node[6]}.getNormalized();
    }

    for (std::size_t place = 0; place < nodes.size(); ++place) {
        const std::uint32_t index = nodes[place].index;
        const std::span<const float> node = values.subspan(place * kTransformFloats, kTransformFloats);
        Slot& slot = slots[index];
        slot.position = {node[0], node[1], node[2]};
        slot.rotation = math::Quat{node[3], node[4], node[5], node[6]}.getNormalized();
        touch(index);
    }
}

void NodeTree3D::readTransforms(std::span<const Node3D> nodes, std::span<float> values) {
    if (values.size() < nodes.size() * kTransformFloats) {
        throw std::invalid_argument("Node3D transforms take seven floats for each node.");
    }

    refresh(nullptr);
    for (std::size_t place = 0; place < nodes.size(); ++place) {
        const Slot& slot = slots[check(nodes[place])];
        const math::Vec3 position = slot.world.getTranslation();
        const std::span<float> node = values.subspan(place * kTransformFloats, kTransformFloats);
        node[0] = position.x;
        node[1] = position.y;
        node[2] = position.z;
        node[3] = slot.worldRotation.x;
        node[4] = slot.worldRotation.y;
        node[5] = slot.worldRotation.z;
        node[6] = slot.worldRotation.w;
    }
}

core::Connection NodeTree3D::on(const Node3D& node, NodeEvent3D event, core::Signal<const Node3D&>::Slot slot, core::Signal<const Node3D&>::Options options) {
    (void)check(node);
    std::shared_ptr<Watchers>& watcher = watchers[node.getKey()];
    if (!watcher) {
        watcher = std::make_shared<Watchers>();
    }

    return watcher->signals[static_cast<std::size_t>(event)].connect(std::move(slot), std::move(options));
}

void NodeTree3D::notify(std::uint64_t key, NodeEvent3D event, const Node3D& subject) {
    const auto found = watchers.find(key);
    if (found == watchers.end()) {
        return;
    }

    const std::shared_ptr<Watchers> held = found->second;
    held->signals[static_cast<std::size_t>(event)].emit(subject);
}

std::uint32_t NodeTree3D::check(const Node3D& node) const {
    if (node.tree.lock().get() != this) {
        throw std::logic_error("The node belongs to another tree.");
    }

    if (node.index >= slots.size() || !slots[node.index].alive || details[node.index].generation != node.generation) {
        throw std::logic_error("The node was destroyed.");
    }

    return node.index;
}

Node3D NodeTree3D::handle(std::int32_t index) const {
    const auto slot = static_cast<std::uint32_t>(index);
    return {std::const_pointer_cast<NodeTree3D>(shared_from_this()), slot, details[slot].generation};
}

std::int32_t NodeTree3D::allocate() {
    if (!freeSlots.empty()) {
        const std::int32_t index = freeSlots.back();
        freeSlots.pop_back();
        slots[static_cast<std::size_t>(index)] = Slot{.alive = true};
        return index;
    }

    slots.push_back(Slot{.alive = true});
    details.emplace_back();
    return static_cast<std::int32_t>(slots.size() - 1);
}

void NodeTree3D::link(std::int32_t index, std::int32_t parent) {
    Detail& detail = details[static_cast<std::size_t>(index)];
    slots[static_cast<std::size_t>(index)].parent = parent;
    std::int32_t& first = parent == kNone ? firstRoot : details[static_cast<std::size_t>(parent)].firstChild;
    std::int32_t& last = parent == kNone ? lastRoot : details[static_cast<std::size_t>(parent)].lastChild;
    detail.previous = last;
    detail.next = kNone;
    if (last != kNone) {
        details[static_cast<std::size_t>(last)].next = index;
    } else {
        first = index;
    }

    last = index;
}

void NodeTree3D::unlink(std::int32_t index) noexcept {
    Detail& detail = details[static_cast<std::size_t>(index)];
    const std::int32_t parent = slots[static_cast<std::size_t>(index)].parent;
    std::int32_t& first = parent == kNone ? firstRoot : details[static_cast<std::size_t>(parent)].firstChild;
    std::int32_t& last = parent == kNone ? lastRoot : details[static_cast<std::size_t>(parent)].lastChild;

    if (detail.previous != kNone) {
        details[static_cast<std::size_t>(detail.previous)].next = detail.next;
    } else {
        first = detail.next;
    }

    if (detail.next != kNone) {
        details[static_cast<std::size_t>(detail.next)].previous = detail.previous;
    } else {
        last = detail.previous;
    }

    detail.previous = kNone;
    detail.next = kNone;
}

// Children go first, and a listener that adds a child to a node on its way out sees that child go too. A listener may destroy this node itself, and its slot may then hold a node added since, which stays. The walk keeps its own stack, so a chain of any depth goes without deep recursion, and a listener that destroys other nodes walks above the entries of the walk it interrupts.
void NodeTree3D::release(std::int32_t index) {
    const std::size_t base = releasing.size();
    const StackScope<Releasing> scope(releasing, base);
    releasing.push_back({.index = index, .generation = details[static_cast<std::size_t>(index)].generation});

    while (releasing.size() > base) {
        const Releasing current = releasing.back();
        const auto slot = static_cast<std::size_t>(current.index);
        if (!holds(slot, current.generation)) {
            releasing.pop_back();
            continue;
        }

        const std::int32_t child = details[slot].firstChild;
        if (child != kNone) {
            releasing.push_back({.index = child, .generation = details[static_cast<std::size_t>(child)].generation});
            continue;
        }

        if (!current.announced) {
            releasing.back().announced = true;
            announce(current.index, current.generation);
            continue;
        }

        releasing.pop_back();
        recycle(current.index);
    }
}

// Tells the listeners of the whole tree, then those of the parent and then those of the node that the node goes.
void NodeTree3D::announce(std::int32_t index, std::uint32_t generation) {
    const auto slot = static_cast<std::size_t>(index);
    const Node3D node = handle(index);
    destroyed.emit(node);

    if (holds(slot, generation) && slots[slot].parent != kNone) {
        notify(handle(slots[slot].parent).getKey(), NodeEvent3D::ChildRemoved, node);
    }

    if (const auto watched = watchers.find(node.getKey()); watched != watchers.end() && holds(slot, generation)) {
        const std::shared_ptr<Watchers> held = std::move(watched->second);
        watchers.erase(watched);
        held->signals[static_cast<std::size_t>(NodeEvent3D::Destroyed)].emit(node);
    }
}

void NodeTree3D::recycle(std::int32_t index) {
    const auto slot = static_cast<std::size_t>(index);
    const std::uint32_t generation = details[slot].generation;
    watchers.erase(handle(index).getKey());
    unlink(index);
    dropTags(static_cast<std::uint32_t>(index));
    details[slot] = Detail{.generation = generation + 1};
    slots[slot] = Slot{};
    freeSlots.push_back(index);
    --count;
    orderChanged = true;
}

bool NodeTree3D::holds(std::size_t slot, std::uint32_t generation) const noexcept {
    return slots[slot].alive && details[slot].generation == generation;
}

void NodeTree3D::touch(std::uint32_t index) {
    Slot& slot = slots[index];
    if (slot.changed) {
        return;
    }

    slot.changed = true;
    dirty.push_back(index);
}

// The move keeps the tree as it was when it cannot happen: a cycle, or a world placement that the new parent cannot hold.
void NodeTree3D::setParent(std::uint32_t index, std::int32_t parent, bool keepWorld) {
    for (std::int32_t ancestor = parent; ancestor != kNone; ancestor = slots[static_cast<std::size_t>(ancestor)].parent) {
        if (ancestor == static_cast<std::int32_t>(index)) {
            throw std::invalid_argument("A node cannot move under itself or under one of its descendants.");
        }
    }

    if (keepWorld) {
        refresh(nullptr);
    }

    const Slot& moving = slots[index];
    const math::Vec3 worldPosition = moving.world.getTranslation();
    const math::Quat worldRotation = moving.worldRotation;
    const math::Vec3 worldScale = moving.worldScale;
    if (keepWorld && parent != kNone) {
        const Slot& holder = slots[static_cast<std::size_t>(parent)];
        if ((moving.inheritance.position && !holder.world.isInvertible()) || (moving.inheritance.scale && (holder.worldScale.x == 0.0F || holder.worldScale.y == 0.0F || holder.worldScale.z == 0.0F))) {
            throw std::invalid_argument("A node cannot keep its place in the world under a parent scaled to nothing.");
        }
    }

    const std::int32_t previous = moving.parent;
    const bool wasEnabled = isEnabledAt(static_cast<std::int32_t>(index));
    unlink(static_cast<std::int32_t>(index));
    link(static_cast<std::int32_t>(index), parent);
    orderChanged = true;
    touch(index);
    if (keepWorld) {
        setWorldPosition(index, worldPosition);
        setWorldRotation(index, worldRotation);
        setWorldScale(index, worldScale);
    }

    const Node3D node = handle(static_cast<std::int32_t>(index));
    const std::uint64_t left = previous == kNone ? 0 : handle(previous).getKey();
    const std::uint64_t joined = parent == kNone ? 0 : handle(parent).getKey();
    const bool nowEnabled = isEnabledAt(static_cast<std::int32_t>(index));
    parentChanged.emit(node);
    notify(node.getKey(), NodeEvent3D::ParentChanged, node);
    if (previous != kNone) {
        notify(left, NodeEvent3D::ChildRemoved, node);
    }

    if (parent != kNone) {
        notify(joined, NodeEvent3D::ChildAdded, node);
    }

    if (wasEnabled != nowEnabled && node.isValid()) {
        announceEnabled(index);
    }
}

void NodeTree3D::setEnabled(std::uint32_t index, bool value) {
    Slot& slot = slots[index];
    if (slot.enabled == value) {
        return;
    }

    const bool above = slot.parent == kNone || isEnabledAt(slot.parent);
    slot.enabled = value;
    touch(index);
    if (above) {
        announceEnabled(index);
    }
}

bool NodeTree3D::isEnabledAt(std::int32_t index) const noexcept {
    for (std::int32_t node = index; node != kNone; node = slots[static_cast<std::size_t>(node)].parent) {
        if (!slots[static_cast<std::size_t>(node)].enabled) {
            return false;
        }
    }

    return true;
}

// The nodes that change are gathered before any listener runs, so a listener that changes the tree never disturbs the walk, and a node that went before its turn is skipped.
void NodeTree3D::announceEnabled(std::uint32_t index) {
    const std::size_t base = announcing.size();
    const StackScope<Node3D> scope(announcing, base);
    walk.clear();
    walk.push_back(static_cast<std::int32_t>(index));
    while (!walk.empty()) {
        const std::int32_t node = walk.back();
        walk.pop_back();
        announcing.push_back(handle(node));
        for (std::int32_t child = details[static_cast<std::size_t>(node)].lastChild; child != kNone; child = details[static_cast<std::size_t>(child)].previous) {
            if (slots[static_cast<std::size_t>(child)].enabled) {
                walk.push_back(child);
            }
        }
    }

    const std::size_t end = announcing.size();
    for (std::size_t place = base; place < end; ++place) {
        const Node3D node = announcing[place];
        if (!node.isValid()) {
            continue;
        }

        enabledChanged.emit(node);
        notify(node.getKey(), NodeEvent3D::EnabledChanged, node);
    }
}

// The setters of world parts expect the parent to be up to date, and a part that the node does not inherit is its world part already.
void NodeTree3D::setWorldPosition(std::uint32_t index, math::Vec3 value) {
    Slot& slot = slots[index];
    if (slot.parent == kNone || !slot.inheritance.position) {
        slot.position = value;
        touch(index);
        return;
    }

    const math::Mat4& parent = slots[static_cast<std::size_t>(slot.parent)].world;
    if (!parent.isInvertible()) {
        throw std::invalid_argument("A node under a parent scaled to nothing has no local position for a world position.");
    }

    slot.position = parent.getAffineInverse().apply(value);
    touch(index);
}

void NodeTree3D::setWorldRotation(std::uint32_t index, math::Quat value) {
    Slot& slot = slots[index];
    const math::Quat rotation = value.getNormalized();
    if (slot.parent == kNone || !slot.inheritance.rotation) {
        slot.rotation = rotation;
        touch(index);
        return;
    }

    slot.rotation = (slots[static_cast<std::size_t>(slot.parent)].worldRotation.getInverse() * rotation).getNormalized();
    touch(index);
}

void NodeTree3D::setWorldScale(std::uint32_t index, math::Vec3 value) {
    Slot& slot = slots[index];
    slot.scale = slot.parent == kNone || !slot.inheritance.scale ? value : value / slots[static_cast<std::size_t>(slot.parent)].worldScale;
    touch(index);
}

void NodeTree3D::addTag(std::uint32_t index, std::string_view tag) {
    auto found = tagIds.find(tag);
    if (found == tagIds.end()) {
        found = tagIds.emplace(std::string(tag), static_cast<std::uint32_t>(tagNames.size())).first;
        tagNames.emplace_back(tag);
        tagMembers.emplace_back();
    }

    const std::uint32_t id = found->second;
    Detail& detail = details[index];
    if (std::ranges::find(detail.tags, id, &std::pair<std::uint32_t, std::uint32_t>::first) != detail.tags.end()) {
        return;
    }

    std::vector<std::uint32_t>& members = tagMembers[id];
    detail.tags.emplace_back(id, static_cast<std::uint32_t>(members.size()));
    members.push_back(index);
}

void NodeTree3D::removeTag(std::uint32_t index, std::string_view tag) {
    const auto found = tagIds.find(tag);
    if (found != tagIds.end()) {
        removeTagId(index, found->second);
    }
}

// The last member of the tag takes the place of the node that leaves, so a removal costs the same however many nodes share the tag.
void NodeTree3D::removeTagId(std::uint32_t index, std::uint32_t id) noexcept {
    std::vector<std::pair<std::uint32_t, std::uint32_t>>& tags = details[index].tags;
    const auto entry = std::ranges::find(tags, id, &std::pair<std::uint32_t, std::uint32_t>::first);
    if (entry == tags.end()) {
        return;
    }

    std::vector<std::uint32_t>& members = tagMembers[id];
    const std::uint32_t place = entry->second;
    const std::uint32_t moved = members.back();
    members[place] = moved;
    members.pop_back();
    if (moved != index) {
        std::vector<std::pair<std::uint32_t, std::uint32_t>>& movedTags = details[moved].tags;
        std::ranges::find(movedTags, id, &std::pair<std::uint32_t, std::uint32_t>::first)->second = place;
    }

    *entry = tags.back();
    tags.pop_back();
}

void NodeTree3D::dropTags(std::uint32_t index) noexcept {
    std::vector<std::pair<std::uint32_t, std::uint32_t>>& tags = details[index].tags;
    while (!tags.empty()) {
        removeTagId(index, tags.back().first);
    }
}

std::int32_t NodeTree3D::findChild(std::int32_t parent, std::string_view name) const {
    std::int32_t child = parent == kNone ? firstRoot : details[static_cast<std::size_t>(parent)].firstChild;
    while (child != kNone && details[static_cast<std::size_t>(child)].name != name) {
        child = details[static_cast<std::size_t>(child)].next;
    }

    return child;
}

std::int32_t NodeTree3D::findPath(std::int32_t from, std::string_view path) const {
    std::int32_t current = from;
    while (!path.empty()) {
        const std::size_t slash = path.find('/');
        current = findChild(current, path.substr(0, slash));
        if (current == kNone) {
            return kNone;
        }

        path = slash == std::string_view::npos ? std::string_view{} : path.substr(slash + 1);
    }

    return current;
}

void NodeTree3D::rebuildOrder() {
    order.clear();
    places.resize(slots.size());

    // The children of a node go on the stack last to first, so the walk visits them in order.
    walk.clear();
    for (std::int32_t root = lastRoot; root != kNone; root = details[static_cast<std::size_t>(root)].previous) {
        walk.push_back(root);
    }

    while (!walk.empty()) {
        const auto index = static_cast<std::uint32_t>(walk.back());
        walk.pop_back();
        places[index] = static_cast<std::uint32_t>(order.size());
        order.push_back(index);
        for (std::int32_t child = details[index].lastChild; child != kNone; child = details[static_cast<std::size_t>(child)].previous) {
            walk.push_back(child);
        }
    }

    // A subtree is contiguous in the order, so it ends where the last of its descendants ends.
    subtreeEnds.assign(order.size(), 0U);
    for (std::size_t place = order.size(); place-- > 0;) {
        subtreeEnds[place] = std::max(subtreeEnds[place], static_cast<std::uint32_t>(place + 1));
        const std::int32_t parent = slots[order[place]].parent;
        if (parent != kNone) {
            std::uint32_t& end = subtreeEnds[places[static_cast<std::size_t>(parent)]];
            end = std::max(end, subtreeEnds[place]);
        }
    }

    orderChanged = false;
}

} // namespace haylen::nodes3d
