#include "haylen/2d/nodes/NodeTree.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/particles/Emitter.hpp"
#include "haylen/2d/particles/System.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/text/Font.hpp"

namespace haylen::nodes2d {

std::shared_ptr<NodeTree> NodeTree::create() {
    return std::make_shared<NodeTree>(Passkey{});
}

NodeTree::Placement NodeTree::placementOf(const math::Transform2D& transform) noexcept {
    const float scaleX = std::hypot(transform.a, transform.b);
    const float scaleY = scaleX > 0.0F ? transform.getDeterminant() / scaleX : std::hypot(transform.c, transform.d);
    return {.position = transform.getTranslation(), .rotation = std::atan2(transform.b, transform.a), .scale = {scaleX, scaleY}};
}

math::Rect NodeTree::transformBounds(const math::Transform2D& transform, const math::Rect& area) noexcept {
    const std::array<math::Vec2, 4> corners{transform.apply(area.getMin()), transform.apply(math::Vec2{area.getRight(), area.y}), transform.apply(area.getMax()), transform.apply(math::Vec2{area.x, area.getBottom()})};
    return math::Geometry::bounds(corners);
}

bool NodeTree::isParticles(const NodeContent& content) noexcept {
    return std::holds_alternative<std::shared_ptr<particles2d::Emitter>>(content.value) || std::holds_alternative<std::shared_ptr<particles2d::System>>(content.value);
}

std::optional<math::Rect> NodeTree::measure(const NodeContent& content) {
    if (const auto* sprite = std::get_if<graphics2d::Sprite>(&content.value)) {
        if (!sprite->texture.isValid()) {
            throw std::invalid_argument("A sprite node needs a texture.");
        }
        const math::Vec2 source = sprite->source.isEmpty() ? sprite->texture.getSize() : sprite->source.getSize();
        const math::Vec2 size = sprite->size.isZero() ? source : sprite->size;
        return transformBounds(math::Transform2D::compose(sprite->position, sprite->rotation, sprite->scale), {-sprite->pivot.x * size.x, -sprite->pivot.y * size.y, size.x, size.y});
    }
    if (const auto* text = std::get_if<NodeContent::Text>(&content.value)) {
        if (!text->font) {
            throw std::invalid_argument("A text node needs a font.");
        }
        const math::Vec2 size = text->font->measure(text->content, text->style);
        const math::Vec2 block = size / text->style.scale;
        return transformBounds(math::Transform2D::compose({}, text->style.rotation, text->style.scale), {-text->style.anchor.x * block.x, -text->style.anchor.y * block.y, block.x, block.y});
    }
    if (const auto* frame = std::get_if<NodeContent::Frame>(&content.value)) {
        if (!frame->slice.isValid()) {
            throw std::invalid_argument("A frame node needs a nine-slice with a texture.");
        }
        if (!(frame->size.x >= 0.0F && frame->size.y >= 0.0F && frame->borderScale > 0.0F)) {
            throw std::invalid_argument("A frame node needs a size of at least 0 and a positive border scale.");
        }
        return math::Rect{-frame->pivot.x * frame->size.x, -frame->pivot.y * frame->size.y, frame->size.x, frame->size.y};
    }
    if (const auto* emitter = std::get_if<std::shared_ptr<particles2d::Emitter>>(&content.value); emitter != nullptr && !*emitter) {
        throw std::invalid_argument("A particle node needs an emitter.");
    }
    if (const auto* system = std::get_if<std::shared_ptr<particles2d::System>>(&content.value); system != nullptr && !*system) {
        throw std::invalid_argument("A particle node needs a particle system.");
    }
    return std::nullopt;
}

Node NodeTree::add(const NodeProperties& properties) {
    return insert(kNone, properties);
}

Node NodeTree::add(const Node& parent, const NodeProperties& properties) {
    return insert(static_cast<std::int32_t>(check(parent)), properties);
}

Node NodeTree::insert(std::int32_t parent, const NodeProperties& properties) {
    if (count >= kMaxNodes) {
        throw std::length_error("A node tree holds at most 16777216 nodes.");
    }
    const std::optional<math::Rect> bounds = measure(properties.content);

    const std::int32_t index = allocate();
    Slot& slot = slots[static_cast<std::size_t>(index)];
    slot.position = properties.position;
    slot.rotation = properties.rotation;
    slot.scale = properties.scale;
    slot.color = properties.color;
    slot.visible = properties.visible;
    slot.layer = properties.layer;
    slot.depth = properties.depth;
    slot.sortOffset = properties.sortOffset;
    slot.bounded = bounds.has_value();
    slot.localBounds = bounds.value_or(math::Rect{});

    Detail& detail = details[static_cast<std::size_t>(index)];
    detail.name = properties.name;
    detail.content = properties.content;
    link(index, parent);
    ++count;
    orderChanged = true;
    pending = true;
    return handle(index);
}

Node NodeTree::find(std::string_view path) const {
    const std::int32_t found = findPath(kNone, path);
    return found == kNone ? Node{} : handle(found);
}

std::vector<Node> NodeTree::getRoots() const {
    std::vector<Node> roots;
    for (std::int32_t index = firstRoot; index != kNone; index = details[static_cast<std::size_t>(index)].next) {
        roots.push_back(handle(index));
    }
    return roots;
}

void NodeTree::clear() {
    while (firstRoot != kNone) {
        release(firstRoot);
    }
}

void NodeTree::update(float deltaSeconds, float blend, core::JobSystem* jobs) {
    lastBlend = blend;
    refresh(jobs);
    advanceParticles(deltaSeconds, jobs);
}

void NodeTree::refresh(core::JobSystem* jobs) {
    if (orderChanged) {
        rebuildOrder();
    }

    // Followers move with their bodies every pass, so a pass runs whenever one exists.
    if (!pending && followers == 0) {
        return;
    }
    pending = false;
    if (jobs != nullptr && count >= kParallelNodes && chunks.size() > 1) {
        // clang-format off
        jobs->parallelFor(0, chunks.size(), 1, [this](std::size_t begin, std::size_t end) {
            for (std::size_t chunk = begin; chunk < end; ++chunk) {
                updateRange(chunks[chunk].first, chunks[chunk].second, lastBlend);
            }
        });
        // clang-format on
        return;
    }
    updateRange(0, order.size(), lastBlend);
}

void NodeTree::follow(Slot& slot, const physics2d::Body& body, float blend) const {
    const physics2d::World& world = *body.getWorld();
    const physics2d::World::Transform placed = world.isInterpolating() ? world.getInterpolatedTransform(body, blend) : physics2d::World::Transform{body.getPosition(), body.getRotation()};
    if (slot.parent == kNone) {
        slot.position = placed.position;
        slot.rotation = placed.rotation;
    } else {
        const math::Transform2D& parent = slots[static_cast<std::size_t>(slot.parent)].world;
        slot.position = parent.getInverse().apply(placed.position);
        slot.rotation = placed.rotation - placementOf(parent).rotation;
    }
    slot.changed = true;
}

void NodeTree::updateRange(std::size_t begin, std::size_t end, float blend) {
    for (std::size_t place = begin; place < end; ++place) {
        const std::uint32_t index = order[place];
        Slot& slot = slots[index];
        if (slot.following) {
            const physics2d::Body& body = details[index].followed;
            if (body.isValid()) {
                follow(slot, body, blend);
            }
        }

        const Slot* parent = slot.parent == kNone ? nullptr : &slots[static_cast<std::size_t>(slot.parent)];
        if (!slot.changed && (parent == nullptr || !parent->moved)) {
            slot.moved = false;
            continue;
        }

        const math::Transform2D local = math::Transform2D::compose(slot.position, slot.rotation, slot.scale);
        slot.world = parent != nullptr ? parent->world * local : local;
        slot.worldColor = parent != nullptr ? parent->worldColor * slot.color : slot.color;
        slot.worldVisible = slot.visible && (parent == nullptr || parent->worldVisible);
        slot.worldLayer = slot.layer + (parent != nullptr ? parent->worldLayer : 0);
        if (slot.bounded) {
            slot.bounds = transformBounds(slot.world, slot.localBounds);
        }
        slot.changed = false;
        slot.moved = true;
    }
}

// Particles are born at the place of their node, turned and scaled with it.
void NodeTree::advanceParticles(float deltaSeconds, core::JobSystem* jobs) {
    for (const std::uint32_t index : particles) {
        const Slot& slot = slots[index];
        const Placement placement = placementOf(slot.world);
        const float scale = std::sqrt(std::fabs(slot.world.getDeterminant()));
        if (const auto* emitter = std::get_if<std::shared_ptr<particles2d::Emitter>>(&details[index].content.value)) {
            particles2d::Emitter& target = **emitter;
            target.position = placement.position;
            target.rotation = placement.rotation;
            target.scale = scale;
            if (jobs != nullptr) {
                target.update(deltaSeconds, *jobs);
            } else {
                target.update(deltaSeconds);
            }
        } else if (const auto* system = std::get_if<std::shared_ptr<particles2d::System>>(&details[index].content.value)) {
            particles2d::System& target = **system;
            target.position = placement.position;
            target.rotation = placement.rotation;
            target.scale = scale;
            if (jobs != nullptr) {
                target.update(deltaSeconds, *jobs);
            } else {
                target.update(deltaSeconds);
            }
        }
    }
}

void NodeTree::draw(graphics2d::Renderer& renderer) {
    const math::Rect view = renderer.getCanvasBounds();
    refresh(nullptr);
    drawn = 0;
    culled = 0;

    // A hidden node skips its whole subtree.
    std::size_t place = 0;
    while (place < order.size()) {
        const std::uint32_t index = order[place];
        const Slot& slot = slots[index];
        if (!slot.worldVisible) {
            place = subtreeEnds[place];
            continue;
        }
        ++place;
        if (details[index].content.isEmpty()) {
            continue;
        }
        if (slot.bounded && !slot.bounds.intersects(view)) {
            ++culled;
            continue;
        }
        drawNode(renderer, index);
        ++drawn;
    }
    flushSprites(renderer);
    runTexture = {};
    runOrder = {};
}

void NodeTree::flushSprites(graphics2d::Renderer& renderer) {
    if (!run.empty()) {
        renderer.drawBatch(runTexture, run, runOrder);
        run.clear();
    }
}

// Neighbouring sprites of one texture and order gather into a run that draws as one batch, and everything else draws on its own after the run before it.
void NodeTree::drawNode(graphics2d::Renderer& renderer, std::uint32_t index) {
    const Slot& slot = slots[index];
    const NodeContent& content = details[index].content;
    // clang-format off
    const auto place = [&slot](graphics2d::DrawOrder placed) {
        placed.layer += slot.worldLayer;
        placed.depth += slot.depth;
        placed.sortOffset += slot.sortOffset;
        return placed;
    };
    // clang-format on

    if (const auto* sprite = std::get_if<graphics2d::Sprite>(&content.value)) {
        const Placement placement = placementOf(slot.world * math::Transform2D::compose(sprite->position, sprite->rotation, sprite->scale));
        const graphics2d::DrawOrder placedOrder = place(sprite->order);
        if (sprite->effect.isActive() || placedOrder.partMask.isValid()) {
            flushSprites(renderer);
            graphics2d::Sprite placed = *sprite;
            placed.position = placement.position;
            placed.rotation = placement.rotation;
            placed.scale = placement.scale;
            placed.color = sprite->color * slot.worldColor;
            placed.order = placedOrder;
            renderer.draw(placed);
            return;
        }
        if (!run.empty() && (!(runTexture == sprite->texture) || !(runOrder == placedOrder))) {
            flushSprites(renderer);
        }
        const math::Rect source = sprite->source.isEmpty() ? math::Rect{0.0F, 0.0F, sprite->texture.getSize().x, sprite->texture.getSize().y} : sprite->source;
        runTexture = sprite->texture;
        runOrder = placedOrder;
        run.push_back({.position = placement.position, .size = (sprite->size.isZero() ? source.getSize() : sprite->size) * placement.scale, .source = source, .pivot = sprite->pivot, .rotation = placement.rotation, .color = sprite->color * slot.worldColor, .flash = sprite->flash, .flip = sprite->flip});
        return;
    }

    flushSprites(renderer);
    if (const auto* text = std::get_if<NodeContent::Text>(&content.value)) {
        const Placement placement = placementOf(slot.world * math::Transform2D::compose({}, text->style.rotation, text->style.scale));
        text::Style style = text->style;
        style.rotation = placement.rotation;
        style.scale = placement.scale;
        style.color = style.color * slot.worldColor;
        style.outlineColor = style.outlineColor.withAlpha(style.outlineColor.a * slot.worldColor.a);
        style.shadowColor = style.shadowColor.withAlpha(style.shadowColor.a * slot.worldColor.a);
        renderer.drawText(*text->font, text->content, placement.position, style, place(text->order));
    } else if (const auto* frame = std::get_if<NodeContent::Frame>(&content.value)) {
        // Every piece of the frame is a sprite turned with the node, so frames turn and scale like sprites.
        patches.clear();
        frame->slice.layout(slot.localBounds, frame->borderScale, patches);
        const Placement placement = placementOf(slot.world);
        pieces.clear();
        for (const graphics2d::NineSlice::Patch& patch : patches) {
            pieces.push_back({.position = slot.world.apply(patch.area.getCenter()), .size = patch.area.getSize() * placement.scale, .source = patch.source, .rotation = placement.rotation, .color = frame->color * slot.worldColor});
        }
        renderer.drawBatch(frame->slice.texture, pieces, place(frame->order));
    } else if (slot.worldLayer != 0) {
        renderer.pushLayerOffset(slot.worldLayer);
        drawParticles(renderer, content);
        renderer.popLayerOffset();
    } else {
        drawParticles(renderer, content);
    }
}

void NodeTree::drawParticles(graphics2d::Renderer& renderer, const NodeContent& content) {
    if (const auto* emitter = std::get_if<std::shared_ptr<particles2d::Emitter>>(&content.value)) {
        (*emitter)->draw(renderer);
    } else if (const auto* system = std::get_if<std::shared_ptr<particles2d::System>>(&content.value)) {
        (*system)->draw(renderer);
    }
}

core::Connection NodeTree::onDestroyed(const Node& node, core::Signal<const Node&>::Slot slot, core::Signal<const Node&>::Options options) {
    (void)check(node);
    std::unique_ptr<core::Signal<const Node&>>& watcher = watchers[node.getKey()];
    if (!watcher) {
        watcher = std::make_unique<core::Signal<const Node&>>();
    }
    return watcher->connect(std::move(slot), std::move(options));
}

void NodeTree::writeTransforms(std::span<const Node> nodes, std::span<const float> values) {
    if (values.size() != nodes.size() * 3) {
        throw std::invalid_argument("The values hold three floats for each node: x, y and rotation.");
    }
    for (std::size_t place = 0; place < nodes.size(); ++place) {
        const std::uint32_t index = check(nodes[place]);
        Slot& slot = slots[index];
        slot.position = {values[place * 3], values[place * 3 + 1]};
        slot.rotation = values[place * 3 + 2];
        touch(index);
    }
}

void NodeTree::readTransforms(std::span<const Node> nodes, std::span<float> values) const {
    if (values.size() != nodes.size() * 3) {
        throw std::invalid_argument("The values hold three floats for each node: x, y and rotation.");
    }
    for (std::size_t place = 0; place < nodes.size(); ++place) {
        const Slot& slot = slots[check(nodes[place])];
        const Placement placement = placementOf(slot.world);
        values[place * 3] = placement.position.x;
        values[place * 3 + 1] = placement.position.y;
        values[place * 3 + 2] = placement.rotation;
    }
}

std::uint32_t NodeTree::check(const Node& node) const {
    if (node.tree.lock().get() != this) {
        throw std::logic_error("The node belongs to another tree.");
    }
    if (node.index >= slots.size() || !slots[node.index].alive || details[node.index].generation != node.generation) {
        throw std::logic_error("The node was destroyed.");
    }
    return node.index;
}

Node NodeTree::handle(std::int32_t index) const {
    const auto slot = static_cast<std::uint32_t>(index);
    return {std::const_pointer_cast<NodeTree>(shared_from_this()), slot, details[slot].generation};
}

std::int32_t NodeTree::allocate() {
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

void NodeTree::link(std::int32_t index, std::int32_t parent) {
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

void NodeTree::unlink(std::int32_t index) noexcept {
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

// Children go first, and a listener that adds a child to a node on its way out sees that child go too.
void NodeTree::release(std::int32_t index) {
    const auto slot = static_cast<std::size_t>(index);
    while (details[slot].firstChild != kNone) {
        release(details[slot].firstChild);
    }
    const std::uint32_t generation = details[slot].generation;
    const Node node = handle(index);
    destroyed.emit(node);
    if (const auto watched = watchers.find(node.getKey()); watched != watchers.end() && slots[slot].alive && details[slot].generation == generation) {
        const std::unique_ptr<core::Signal<const Node&>> watcher = std::move(watched->second);
        watchers.erase(watched);
        watcher->emit(node);
    }
    if (!slots[slot].alive || details[slot].generation != generation) {
        return;
    }
    while (details[slot].firstChild != kNone) {
        release(details[slot].firstChild);
    }

    unlink(index);
    if (slots[slot].following) {
        --followers;
    }
    details[slot] = Detail{.generation = generation + 1};
    slots[slot] = Slot{};
    freeSlots.push_back(index);
    --count;
    orderChanged = true;
    pending = true;
}

void NodeTree::touch(std::uint32_t index) noexcept {
    slots[index].changed = true;
    pending = true;
}

void NodeTree::setContent(std::uint32_t index, NodeContent content) {
    const std::optional<math::Rect> bounds = measure(content);
    Detail& detail = details[index];
    if (isParticles(detail.content) != isParticles(content)) {
        orderChanged = true;
    }
    detail.content = std::move(content);
    slots[index].bounded = bounds.has_value();
    slots[index].localBounds = bounds.value_or(math::Rect{});
    touch(index);
}

void NodeTree::setParent(std::uint32_t index, std::int32_t parent) {
    for (std::int32_t ancestor = parent; ancestor != kNone; ancestor = slots[static_cast<std::size_t>(ancestor)].parent) {
        if (ancestor == static_cast<std::int32_t>(index)) {
            throw std::invalid_argument("A node cannot move under itself or under one of its descendants.");
        }
    }
    unlink(static_cast<std::int32_t>(index));
    link(static_cast<std::int32_t>(index), parent);
    orderChanged = true;
    touch(index);
}

std::int32_t NodeTree::findChild(std::int32_t parent, std::string_view name) const {
    std::int32_t child = parent == kNone ? firstRoot : details[static_cast<std::size_t>(parent)].firstChild;
    while (child != kNone && details[static_cast<std::size_t>(child)].name != name) {
        child = details[static_cast<std::size_t>(child)].next;
    }
    return child;
}

std::int32_t NodeTree::findPath(std::int32_t from, std::string_view path) const {
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

void NodeTree::rebuildOrder() {
    order.clear();
    particles.clear();
    chunks.clear();
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
        if (isParticles(details[index].content)) {
            particles.push_back(index);
        }
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

    // Consecutive root subtrees gather into chunks of at least the grain, which update on their own.
    std::size_t begin = 0;
    std::size_t place = 0;
    while (place < order.size()) {
        place = subtreeEnds[place];
        if (place - begin >= kParallelGrain || place == order.size()) {
            chunks.emplace_back(begin, place);
            begin = place;
        }
    }
    orderChanged = false;
}

} // namespace haylen::nodes2d
