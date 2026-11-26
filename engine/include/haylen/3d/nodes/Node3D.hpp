#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/3d/nodes/Inheritance3D.hpp"
#include "haylen/3d/nodes/NodeEvent3D.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Transform3D.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::nodes3d {

class Component3D;
class ComponentType3D;
class NodeTree3D;
struct NodeProperties3D;

// Handle to a node of a 3D tree. It reports `isValid()` while its node and its tree exist, and every other method throws `std::logic_error` once the node was destroyed or its tree is gone. Two handles are equal when they refer to the same node. The world state that a handle reads is always current: a read first brings the subtrees that changed since the last read up to date.
class Node3D final {
  public:
    Node3D() = default;

    [[nodiscard]] bool isValid() const noexcept;

    // A number that names the node among every node its tree ever held, which never repeats.
    [[nodiscard]] std::uint64_t getKey() const noexcept {
        return (static_cast<std::uint64_t>(generation) << 32U) | index;
    }

    [[nodiscard]] NodeTree3D& getTree() const;

    [[nodiscard]] const std::string& getName() const;
    void setName(std::string value);

    // The local transform in meters, relative to the parent by the parts the node inherits. A rotation is normalized as it is set, and one without a length throws `std::invalid_argument`.
    [[nodiscard]] math::Vec3 getPosition() const;
    void setPosition(math::Vec3 value);
    [[nodiscard]] float getX() const;
    void setX(float value);
    [[nodiscard]] float getY() const;
    void setY(float value);
    [[nodiscard]] float getZ() const;
    void setZ(float value);
    [[nodiscard]] math::Quat getRotation() const;
    void setRotation(math::Quat value);
    [[nodiscard]] math::Vec3 getScale() const;
    void setScale(math::Vec3 value);
    [[nodiscard]] math::Transform3D getTransform() const;
    void setTransform(const math::Transform3D& value);

    [[nodiscard]] bool isVisible() const;
    void setVisible(bool value);
    [[nodiscard]] bool isEnabled() const;
    void setEnabled(bool value);
    [[nodiscard]] std::uint32_t getLayers() const;
    void setLayers(std::uint32_t value);
    [[nodiscard]] bool isStatic() const;
    void setStatic(bool value);
    [[nodiscard]] bool hasFrustumCulling() const;
    void setFrustumCulling(bool value);
    [[nodiscard]] int getRenderOrder() const;
    void setRenderOrder(int value);
    [[nodiscard]] Inheritance3D getInheritance() const;
    void setInheritance(Inheritance3D value);

    // The data of the app, a JSON value that the tree keeps with the node and that a clone copies.
    [[nodiscard]] const core::Json& getData() const;
    void setData(core::Json value);

    // Tags group nodes across the tree, such as every enemy, and `NodeTree3D::findTagged` finds the nodes of a tag. Adding a tag the node has or removing one it lacks changes nothing.
    void addTag(std::string_view tag);
    void removeTag(std::string_view tag);
    [[nodiscard]] bool hasTag(std::string_view tag) const;
    [[nodiscard]] std::vector<std::string> getTags() const;

    // The parent is invalid for a node at the root of its tree. A node moves under a new parent after the children it has, keeping its local transform, or its world position, rotation and scale with `keepWorld`. A node cannot move under itself or under one of its descendants, which throws `std::invalid_argument` and leaves the tree as it was.
    [[nodiscard]] Node3D getParent() const;
    void setParent(const Node3D& value, bool keepWorld = false);
    [[nodiscard]] std::vector<Node3D> getChildren() const;
    [[nodiscard]] std::size_t getChildCount() const;

    // Adds a child after the children the node has.
    Node3D add(const NodeProperties3D& properties) const;

    // Copies the node and its descendants under the same parent, after its siblings, as `NodeTree3D::clone` does.
    Node3D clone() const;

    // Finds a descendant by a path of names separated by slashes, such as `turret/barrel`, or returns an invalid node.
    [[nodiscard]] Node3D find(std::string_view path) const;

    // Returns the path of names from the root of the tree, such as `tank/turret`.
    [[nodiscard]] std::string getPath() const;

    // The world matrix places the node exactly, with the shear that a parent scaled differently on each axis gives a turned child. The world rotation and scale combine the rotations and the scales of the chain the node inherits and leave that shear out. Setting the world position or rotation sets the local part that gives it, which throws `std::invalid_argument` under a parent scaled to nothing.
    [[nodiscard]] math::Mat4 getWorldMatrix() const;
    [[nodiscard]] math::Vec3 getWorldPosition() const;
    void setWorldPosition(math::Vec3 value);
    [[nodiscard]] math::Quat getWorldRotation() const;
    void setWorldRotation(math::Quat value);
    [[nodiscard]] math::Vec3 getWorldScale() const;

    // Whether the node and every ancestor are visible and enabled, and whether they are all enabled.
    [[nodiscard]] bool isWorldVisible() const;
    [[nodiscard]] bool isWorldEnabled() const;

    // Converts points between the space of the node and the world through the world matrix. Converting into a node scaled to nothing throws `std::invalid_argument`.
    [[nodiscard]] math::Vec3 toWorld(math::Vec3 point) const;
    [[nodiscard]] math::Vec3 toLocal(math::Vec3 point) const;

    // Turns the node in the world so its forward axis, negative `z`, points at the target, with its up axis as close to `up` as it can. Throws `std::invalid_argument` for a target at the place of the node, within the tolerance of `Vec3::approximately`, or straight along `up` from it.
    void lookAt(math::Vec3 target, math::Vec3 up = math::Vec3::up());

    // Adds a component of the type from its options and returns it, with the errors of `ComponentStore3D::add`. The component that a node holds of a type is found with `findComponent`, which returns an invalid component for a node without one.
    Component3D addComponent(const std::shared_ptr<const ComponentType3D>& type, const core::Json& options = core::Json::object()) const;
    [[nodiscard]] Component3D findComponent(const std::shared_ptr<const ComponentType3D>& type) const;
    [[nodiscard]] std::vector<Component3D> getComponents() const;

    // Calls the slot on every event of the kind for this node, until the connection ends or the node goes.
    core::Connection on(NodeEvent3D event, core::Signal<const Node3D&>::Slot slot, core::Signal<const Node3D&>::Options options = {}) const;

    // Destroys the node and its descendants, deepest first.
    void destroy();

    friend bool operator==(const Node3D& lhs, const Node3D& rhs) noexcept {
        return lhs.index == rhs.index && lhs.generation == rhs.generation && !lhs.tree.owner_before(rhs.tree) && !rhs.tree.owner_before(lhs.tree);
    }

  private:
    friend class NodeTree3D;

    Node3D(std::weak_ptr<NodeTree3D> owner, std::uint32_t slot, std::uint32_t slotGeneration) noexcept : tree(std::move(owner)), index(slot), generation(slotGeneration) {}

    // Returns the tree with the node checked, which stays alive while the result lives.
    [[nodiscard]] std::shared_ptr<NodeTree3D> lock() const;

    std::weak_ptr<NodeTree3D> tree;
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
};

} // namespace haylen::nodes3d
