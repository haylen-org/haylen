#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/nodes/NodeContent.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {
class Body;
}

namespace haylen::nodes2d {

class NodeTree;
struct NodeProperties;

// Handle to a node of a tree. It reports `isValid()` while its node and its tree exist, and every other method throws `std::logic_error` once the node was destroyed or its tree is gone. Two handles are equal when they refer to the same node.
class Node final {
  public:
    Node() = default;

    [[nodiscard]] bool isValid() const noexcept;

    // A number that names the node among every node its tree ever held, which never repeats.
    [[nodiscard]] std::uint64_t getKey() const noexcept {
        return (static_cast<std::uint64_t>(generation) << 32U) | index;
    }

    [[nodiscard]] NodeTree& getTree() const;

    [[nodiscard]] const std::string& getName() const;
    void setName(std::string value);
    [[nodiscard]] math::Vec2 getPosition() const;
    void setPosition(math::Vec2 value);
    [[nodiscard]] float getX() const;
    void setX(float value);
    [[nodiscard]] float getY() const;
    void setY(float value);
    [[nodiscard]] float getRotation() const;
    void setRotation(float value);
    [[nodiscard]] math::Vec2 getScale() const;
    void setScale(math::Vec2 value);
    [[nodiscard]] float getScaleX() const;
    void setScaleX(float value);
    [[nodiscard]] float getScaleY() const;
    void setScaleY(float value);
    [[nodiscard]] math::Color getColor() const;
    void setColor(math::Color value);
    [[nodiscard]] bool isVisible() const;
    void setVisible(bool value);
    [[nodiscard]] int getLayer() const;
    void setLayer(int value);
    [[nodiscard]] float getDepth() const;
    void setDepth(float value);
    [[nodiscard]] float getSortOffset() const;
    void setSortOffset(float value);
    [[nodiscard]] const NodeContent& getContent() const;
    void setContent(NodeContent value);

    // The sprite of a node that shows one, which the setter changes in place, such as when an animator shows a new frame. Both throw `std::logic_error` for a node without a sprite.
    [[nodiscard]] const graphics2d::Sprite& getSprite() const;
    void setSprite(const graphics2d::Sprite& value);

    // Makes the node take the position and rotation of a body in the world on every update, between its last two steps when its world interpolates, or stops following with an invalid body.
    void follow(const physics2d::Body& body);
    [[nodiscard]] physics2d::Body getFollowed() const;

    // The parent is invalid for a node at the root of its tree, and moving a node under a new parent keeps its local transform. A node cannot move under itself or under one of its descendants.
    [[nodiscard]] Node getParent() const;
    void setParent(const Node& value);
    [[nodiscard]] std::vector<Node> getChildren() const;
    [[nodiscard]] std::size_t getChildCount() const;

    // Adds a child after the children the node has.
    Node add(const NodeProperties& properties) const;

    // Finds a descendant by a path of names separated by slashes, such as `turret/barrel`, or returns an invalid node.
    [[nodiscard]] Node find(std::string_view path) const;

    // Returns the path of names from the root of the tree, such as `tank/turret`.
    [[nodiscard]] std::string getPath() const;

    // The world state of the node as of the last update of its tree: its transform, color, layer and whether it and every ancestor are visible, and the conversions of points between the node and the world through the transform.
    [[nodiscard]] math::Transform2D getWorldTransform() const;
    [[nodiscard]] math::Color getWorldColor() const;
    [[nodiscard]] int getWorldLayer() const;
    [[nodiscard]] bool isWorldVisible() const;
    [[nodiscard]] math::Vec2 toWorld(math::Vec2 point) const;
    [[nodiscard]] math::Vec2 toLocal(math::Vec2 point) const;

    // Destroys the node and its descendants, deepest first.
    void destroy();

    friend bool operator==(const Node& lhs, const Node& rhs) noexcept {
        return lhs.index == rhs.index && lhs.generation == rhs.generation && !lhs.tree.owner_before(rhs.tree) && !rhs.tree.owner_before(lhs.tree);
    }

  private:
    friend class NodeTree;

    Node(std::weak_ptr<NodeTree> owner, std::uint32_t slot, std::uint32_t slotGeneration) noexcept : tree(std::move(owner)), index(slot), generation(slotGeneration) {}

    // Returns the tree with the node checked, which stays alive while the result lives.
    [[nodiscard]] std::shared_ptr<NodeTree> lock() const;

    std::weak_ptr<NodeTree> tree;
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
};

} // namespace haylen::nodes2d
