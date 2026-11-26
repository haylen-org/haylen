#include "haylen/3d/nodes/Node3D.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/3d/nodes/Component3D.hpp"
#include "haylen/3d/nodes/NodeTree3D.hpp"

namespace haylen::nodes3d {

bool Node3D::isValid() const noexcept {
    const std::shared_ptr<NodeTree3D> owner = tree.lock();
    return owner && index < owner->slots.size() && owner->slots[index].alive && owner->details[index].generation == generation;
}

std::shared_ptr<NodeTree3D> Node3D::lock() const {
    std::shared_ptr<NodeTree3D> owner = tree.lock();
    if (!owner) {
        throw std::logic_error("The tree of the node is gone.");
    }

    (void)owner->check(*this);
    return owner;
}

NodeTree3D& Node3D::getTree() const {
    return *lock();
}

const std::string& Node3D::getName() const {
    return lock()->details[index].name;
}

void Node3D::setName(std::string value) {
    lock()->details[index].name = std::move(value);
}

math::Vec3 Node3D::getPosition() const {
    return lock()->slots[index].position;
}

void Node3D::setPosition(math::Vec3 value) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->slots[index].position = value;
    owner->touch(index);
}

float Node3D::getX() const {
    return getPosition().x;
}

void Node3D::setX(float value) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->slots[index].position.x = value;
    owner->touch(index);
}

float Node3D::getY() const {
    return getPosition().y;
}

void Node3D::setY(float value) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->slots[index].position.y = value;
    owner->touch(index);
}

float Node3D::getZ() const {
    return getPosition().z;
}

void Node3D::setZ(float value) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->slots[index].position.z = value;
    owner->touch(index);
}

math::Quat Node3D::getRotation() const {
    return lock()->slots[index].rotation;
}

void Node3D::setRotation(math::Quat value) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->slots[index].rotation = value.getNormalized();
    owner->touch(index);
}

math::Vec3 Node3D::getScale() const {
    return lock()->slots[index].scale;
}

void Node3D::setScale(math::Vec3 value) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->slots[index].scale = value;
    owner->touch(index);
}

math::Transform3D Node3D::getTransform() const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    const NodeTree3D::Slot& slot = owner->slots[index];
    return {.position = slot.position, .rotation = slot.rotation, .scale = slot.scale};
}

void Node3D::setTransform(const math::Transform3D& value) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    const math::Quat rotation = value.rotation.getNormalized();
    NodeTree3D::Slot& slot = owner->slots[index];
    slot.position = value.position;
    slot.rotation = rotation;
    slot.scale = value.scale;
    owner->touch(index);
}

bool Node3D::isVisible() const {
    return lock()->slots[index].visible;
}

void Node3D::setVisible(bool value) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->slots[index].visible = value;
    owner->touch(index);
}

bool Node3D::isEnabled() const {
    return lock()->slots[index].enabled;
}

void Node3D::setEnabled(bool value) {
    lock()->setEnabled(index, value);
}

std::uint32_t Node3D::getLayers() const {
    return lock()->slots[index].layers;
}

void Node3D::setLayers(std::uint32_t value) {
    lock()->slots[index].layers = value;
}

bool Node3D::isStatic() const {
    return lock()->slots[index].isStatic;
}

void Node3D::setStatic(bool value) {
    lock()->slots[index].isStatic = value;
}

bool Node3D::hasFrustumCulling() const {
    return lock()->slots[index].frustumCulling;
}

void Node3D::setFrustumCulling(bool value) {
    lock()->slots[index].frustumCulling = value;
}

int Node3D::getRenderOrder() const {
    return lock()->slots[index].renderOrder;
}

void Node3D::setRenderOrder(int value) {
    lock()->slots[index].renderOrder = value;
}

Inheritance3D Node3D::getInheritance() const {
    return lock()->slots[index].inheritance;
}

void Node3D::setInheritance(Inheritance3D value) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->slots[index].inheritance = value;
    owner->touch(index);
}

const core::Json& Node3D::getData() const {
    return lock()->details[index].data;
}

void Node3D::setData(core::Json value) {
    lock()->details[index].data = std::move(value);
}

void Node3D::addTag(std::string_view tag) {
    lock()->addTag(index, tag);
}

void Node3D::removeTag(std::string_view tag) {
    lock()->removeTag(index, tag);
}

bool Node3D::hasTag(std::string_view tag) const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    const auto found = owner->tagIds.find(tag);
    if (found == owner->tagIds.end()) {
        return false;
    }

    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& tags = owner->details[index].tags;
    return std::ranges::find(tags, found->second, &std::pair<std::uint32_t, std::uint32_t>::first) != tags.end();
}

std::vector<std::string> Node3D::getTags() const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    std::vector<std::string> tags;
    for (const auto& [tag, place] : owner->details[index].tags) {
        tags.push_back(owner->tagNames[tag]);
    }

    return tags;
}

Node3D Node3D::getParent() const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    const std::int32_t parent = owner->slots[index].parent;
    return parent == NodeTree3D::kNone ? Node3D{} : owner->handle(parent);
}

void Node3D::setParent(const Node3D& value, bool keepWorld) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    const std::int32_t parent = value == Node3D{} ? NodeTree3D::kNone : static_cast<std::int32_t>(owner->check(value));
    owner->setParent(index, parent, keepWorld);
}

std::vector<Node3D> Node3D::getChildren() const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    std::vector<Node3D> children;
    for (std::int32_t child = owner->details[index].firstChild; child != NodeTree3D::kNone; child = owner->details[static_cast<std::size_t>(child)].next) {
        children.push_back(owner->handle(child));
    }

    return children;
}

std::size_t Node3D::getChildCount() const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    std::size_t children = 0;
    for (std::int32_t child = owner->details[index].firstChild; child != NodeTree3D::kNone; child = owner->details[static_cast<std::size_t>(child)].next) {
        ++children;
    }

    return children;
}

Node3D Node3D::add(const NodeProperties3D& properties) const {
    return lock()->add(*this, properties);
}

Node3D Node3D::clone() const {
    return lock()->clone(*this, getParent());
}

Node3D Node3D::find(std::string_view path) const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    const std::int32_t found = owner->findPath(static_cast<std::int32_t>(index), path);
    return found == NodeTree3D::kNone ? Node3D{} : owner->handle(found);
}

std::string Node3D::getPath() const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    std::string path = owner->details[index].name;
    for (std::int32_t parent = owner->slots[index].parent; parent != NodeTree3D::kNone; parent = owner->slots[static_cast<std::size_t>(parent)].parent) {
        path = owner->details[static_cast<std::size_t>(parent)].name + "/" + path;
    }

    return path;
}

math::Mat4 Node3D::getWorldMatrix() const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->refresh(nullptr);
    return owner->slots[index].world;
}

math::Vec3 Node3D::getWorldPosition() const {
    return getWorldMatrix().getTranslation();
}

void Node3D::setWorldPosition(math::Vec3 value) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->refresh(nullptr);
    owner->setWorldPosition(index, value);
}

math::Quat Node3D::getWorldRotation() const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->refresh(nullptr);
    return owner->slots[index].worldRotation;
}

void Node3D::setWorldRotation(math::Quat value) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->refresh(nullptr);
    owner->setWorldRotation(index, value);
}

math::Vec3 Node3D::getWorldScale() const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->refresh(nullptr);
    return owner->slots[index].worldScale;
}

bool Node3D::isWorldVisible() const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->refresh(nullptr);
    return owner->slots[index].worldVisible;
}

bool Node3D::isWorldEnabled() const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->refresh(nullptr);
    return owner->slots[index].worldEnabled;
}

math::Vec3 Node3D::toWorld(math::Vec3 point) const {
    return getWorldMatrix().apply(point);
}

math::Vec3 Node3D::toLocal(math::Vec3 point) const {
    const math::Mat4 world = getWorldMatrix();
    if (!world.isInvertible()) {
        throw std::invalid_argument("A node scaled to nothing has no local space to convert a point into.");
    }

    return world.getAffineInverse().apply(point);
}

void Node3D::lookAt(math::Vec3 target, math::Vec3 up) {
    const std::shared_ptr<NodeTree3D> owner = lock();
    owner->refresh(nullptr);

    // A world position carries the rounding of the matrices of the chain, so a target counts as the place of the node within the tolerance of `Vec3::approximately`, the same on every machine.
    const math::Vec3 position = owner->slots[index].world.getTranslation();
    if (math::Vec3::approximately(target, position)) {
        throw std::invalid_argument("A node cannot look at a target at its own place in the world. Give it a target away from the node.");
    }

    owner->setWorldRotation(index, math::Quat::lookRotation(target - position, up));
}

Component3D Node3D::addComponent(const std::shared_ptr<const ComponentType3D>& type, const core::Json& options) const {
    lock()->components.add(*this, type, options);
    return {*this, type};
}

Component3D Node3D::findComponent(const std::shared_ptr<const ComponentType3D>& type) const {
    const std::shared_ptr<NodeTree3D> owner = lock();
    return type && owner->components.has(*this, *type) ? Component3D{*this, type} : Component3D{};
}

std::vector<Component3D> Node3D::getComponents() const {
    std::vector<Component3D> held;
    for (std::shared_ptr<const ComponentType3D>& type : lock()->components.getTypes(*this)) {
        held.emplace_back(*this, std::move(type));
    }

    return held;
}

core::Connection Node3D::on(NodeEvent3D event, core::Signal<const Node3D&>::Slot slot, core::Signal<const Node3D&>::Options options) const {
    return lock()->on(*this, event, std::move(slot), std::move(options));
}

void Node3D::destroy() {
    lock()->release(static_cast<std::int32_t>(index));
}

} // namespace haylen::nodes3d
