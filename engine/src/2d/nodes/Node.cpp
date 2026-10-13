#include "haylen/2d/nodes/Node.hpp"

#include <stdexcept>
#include <utility>

#include "haylen/2d/nodes/NodeTree.hpp"
#include "haylen/2d/physics/Body.hpp"

namespace haylen::nodes2d {

bool Node::isValid() const noexcept {
    const std::shared_ptr<NodeTree> owner = tree.lock();
    return owner && index < owner->slots.size() && owner->slots[index].alive && owner->details[index].generation == generation;
}

std::shared_ptr<NodeTree> Node::lock() const {
    std::shared_ptr<NodeTree> owner = tree.lock();
    if (!owner) {
        throw std::logic_error("The tree of the node is gone.");
    }
    (void)owner->check(*this);
    return owner;
}

NodeTree& Node::getTree() const {
    return *lock();
}

const std::string& Node::getName() const {
    return lock()->details[index].name;
}

void Node::setName(std::string value) {
    lock()->details[index].name = std::move(value);
}

math::Vec2 Node::getPosition() const {
    return lock()->slots[index].position;
}

void Node::setPosition(math::Vec2 value) {
    const std::shared_ptr<NodeTree> owner = lock();
    owner->slots[index].position = value;
    owner->touch(index);
}

float Node::getX() const {
    return getPosition().x;
}

void Node::setX(float value) {
    const std::shared_ptr<NodeTree> owner = lock();
    owner->slots[index].position.x = value;
    owner->touch(index);
}

float Node::getY() const {
    return getPosition().y;
}

void Node::setY(float value) {
    const std::shared_ptr<NodeTree> owner = lock();
    owner->slots[index].position.y = value;
    owner->touch(index);
}

float Node::getRotation() const {
    return lock()->slots[index].rotation;
}

void Node::setRotation(float value) {
    const std::shared_ptr<NodeTree> owner = lock();
    owner->slots[index].rotation = value;
    owner->touch(index);
}

math::Vec2 Node::getScale() const {
    return lock()->slots[index].scale;
}

void Node::setScale(math::Vec2 value) {
    const std::shared_ptr<NodeTree> owner = lock();
    owner->slots[index].scale = value;
    owner->touch(index);
}

float Node::getScaleX() const {
    return getScale().x;
}

void Node::setScaleX(float value) {
    const std::shared_ptr<NodeTree> owner = lock();
    owner->slots[index].scale.x = value;
    owner->touch(index);
}

float Node::getScaleY() const {
    return getScale().y;
}

void Node::setScaleY(float value) {
    const std::shared_ptr<NodeTree> owner = lock();
    owner->slots[index].scale.y = value;
    owner->touch(index);
}

math::Color Node::getColor() const {
    return lock()->slots[index].color;
}

void Node::setColor(math::Color value) {
    const std::shared_ptr<NodeTree> owner = lock();
    owner->slots[index].color = value;
    owner->touch(index);
}

bool Node::isVisible() const {
    return lock()->slots[index].visible;
}

void Node::setVisible(bool value) {
    const std::shared_ptr<NodeTree> owner = lock();
    owner->slots[index].visible = value;
    owner->touch(index);
}

int Node::getLayer() const {
    return lock()->slots[index].layer;
}

void Node::setLayer(int value) {
    const std::shared_ptr<NodeTree> owner = lock();
    owner->slots[index].layer = value;
    owner->touch(index);
}

float Node::getDepth() const {
    return lock()->slots[index].depth;
}

void Node::setDepth(float value) {
    lock()->slots[index].depth = value;
}

float Node::getSortOffset() const {
    return lock()->slots[index].sortOffset;
}

void Node::setSortOffset(float value) {
    lock()->slots[index].sortOffset = value;
}

const NodeContent& Node::getContent() const {
    return lock()->details[index].content;
}

void Node::setContent(NodeContent value) {
    lock()->setContent(index, std::move(value));
}

const graphics2d::Sprite& Node::getSprite() const {
    const auto* sprite = std::get_if<graphics2d::Sprite>(&getContent().value);
    if (sprite == nullptr) {
        throw std::logic_error("The node shows no sprite.");
    }
    return *sprite;
}

void Node::setSprite(const graphics2d::Sprite& value) {
    const std::shared_ptr<NodeTree> owner = lock();
    if (!std::holds_alternative<graphics2d::Sprite>(owner->details[index].content.value)) {
        throw std::logic_error("The node shows no sprite.");
    }
    NodeContent content{.value = value, .path = owner->details[index].content.path};
    owner->setContent(index, std::move(content));
}

void Node::follow(const physics2d::Body& body) {
    const std::shared_ptr<NodeTree> owner = lock();
    NodeTree::Slot& slot = owner->slots[index];
    const bool following = body.isValid();
    if (slot.following != following) {
        owner->followers = following ? owner->followers + 1 : owner->followers - 1;
    }
    slot.following = following;
    owner->details[index].followed = following ? body : physics2d::Body{};
    owner->touch(index);
}

physics2d::Body Node::getFollowed() const {
    return lock()->details[index].followed;
}

Node Node::getParent() const {
    const std::shared_ptr<NodeTree> owner = lock();
    const std::int32_t parent = owner->slots[index].parent;
    return parent == NodeTree::kNone ? Node{} : owner->handle(parent);
}

void Node::setParent(const Node& value) {
    const std::shared_ptr<NodeTree> owner = lock();
    owner->setParent(index, value.generation == 0 ? NodeTree::kNone : static_cast<std::int32_t>(owner->check(value)));
}

std::vector<Node> Node::getChildren() const {
    const std::shared_ptr<NodeTree> owner = lock();
    std::vector<Node> children;
    for (std::int32_t child = owner->details[index].firstChild; child != NodeTree::kNone; child = owner->details[static_cast<std::size_t>(child)].next) {
        children.push_back(owner->handle(child));
    }
    return children;
}

std::size_t Node::getChildCount() const {
    const std::shared_ptr<NodeTree> owner = lock();
    std::size_t children = 0;
    for (std::int32_t child = owner->details[index].firstChild; child != NodeTree::kNone; child = owner->details[static_cast<std::size_t>(child)].next) {
        ++children;
    }
    return children;
}

Node Node::add(const NodeProperties& properties) const {
    return lock()->add(*this, properties);
}

Node Node::find(std::string_view path) const {
    const std::shared_ptr<NodeTree> owner = lock();
    const std::int32_t found = owner->findPath(static_cast<std::int32_t>(index), path);
    return found == NodeTree::kNone ? Node{} : owner->handle(found);
}

std::string Node::getPath() const {
    const std::shared_ptr<NodeTree> owner = lock();
    std::string path = owner->details[index].name;
    for (std::int32_t parent = owner->slots[index].parent; parent != NodeTree::kNone; parent = owner->slots[static_cast<std::size_t>(parent)].parent) {
        path = owner->details[static_cast<std::size_t>(parent)].name + "/" + path;
    }
    return path;
}

math::Transform2D Node::getWorldTransform() const {
    return lock()->slots[index].world;
}

math::Color Node::getWorldColor() const {
    return lock()->slots[index].worldColor;
}

int Node::getWorldLayer() const {
    return lock()->slots[index].worldLayer;
}

bool Node::isWorldVisible() const {
    return lock()->slots[index].worldVisible;
}

math::Vec2 Node::toWorld(math::Vec2 point) const {
    return getWorldTransform().apply(point);
}

math::Vec2 Node::toLocal(math::Vec2 point) const {
    return getWorldTransform().getInverse().apply(point);
}

void Node::destroy() {
    lock()->release(static_cast<std::int32_t>(index));
}

} // namespace haylen::nodes2d
