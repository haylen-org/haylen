#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haylen/3d/nodes/ComponentPool3D.hpp"
#include "haylen/3d/nodes/ComponentRegistry3D.hpp"
#include "haylen/3d/nodes/ComponentType3D.hpp"
#include "haylen/3d/nodes/Node3D.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::nodes3d {

class NodeTree3D;

// The components of the nodes of one tree, one pool for each type that a node of the tree holds. A node holds at most one component of each type. Its components go with it, a clone of the node holds copies of them made from what they save, and they rest while the node is off in the hierarchy.
class ComponentStore3D final {
  public:
    explicit ComponentStore3D(NodeTree3D& owner) noexcept : tree(owner) {}

    ComponentStore3D(const ComponentStore3D&) = delete;
    ComponentStore3D& operator=(const ComponentStore3D&) = delete;

    // Adds a component of the type to a node of the tree from its options. Throws `std::invalid_argument` when the node holds a component of the type already, and the errors of the pool for the options.
    void add(const Node3D& node, const std::shared_ptr<const ComponentType3D>& type, const core::Json& options);

    // Removes the component of the type from the node, or does nothing for a node without one.
    void remove(const Node3D& node, const ComponentType3D& type);

    [[nodiscard]] bool has(const Node3D& node, const ComponentType3D& type) const;

    // Returns the types of the components of the node, in the order the tree first held each type.
    [[nodiscard]] std::vector<std::shared_ptr<const ComponentType3D>> getTypes(const Node3D& node) const;

    // Reads and writes a property of the component of the type that the node holds. Throws `std::invalid_argument` for a node without one, and the errors of the pool.
    [[nodiscard]] core::Json get(const Node3D& node, const ComponentType3D& type, std::string_view property) const;
    void set(const Node3D& node, const ComponentType3D& type, std::string_view property, const core::Json& value);

    // Returns the components of the node as an object of the options of each component by the name of its type, or the options of its component of one type.
    [[nodiscard]] core::Json save(const Node3D& node) const;
    [[nodiscard]] core::Json save(const Node3D& node, const ComponentType3D& type) const;

    // Adds the components of an object that `save` wrote, whose types the registry names. Throws `std::invalid_argument` for a type that the registry lacks, before it adds anything.
    void load(const Node3D& node, const core::Json& components, const ComponentRegistry3D& registry);

    // Runs every pool whose type runs in the phase, with the time of the phase.
    void run(ComponentType3D::Phase phase, float deltaSeconds);

    // The number of components of every type.
    [[nodiscard]] std::size_t size() const noexcept;

  private:
    friend class NodeTree3D;

    // The components of one type: the pool with their state, and the node of each component, whether that node is on, and the index of the component of each node by the key of the node.
    struct Pool {
        std::shared_ptr<const ComponentType3D> type{};
        std::unique_ptr<ComponentPool3D> components{};
        std::vector<Node3D> nodes{};
        std::vector<std::uint8_t> active{};
        std::unordered_map<std::uint64_t, std::size_t> places{};
    };

    [[nodiscard]] Pool* findPool(const ComponentType3D& type) noexcept;
    [[nodiscard]] const Pool* findPool(const ComponentType3D& type) const noexcept;
    [[nodiscard]] std::size_t place(const Pool& pool, const Node3D& node) const;
    void removeAt(Pool& pool, std::size_t index);

    // The tree calls these as its nodes go, are copied and turn on or off.
    void removeNode(const Node3D& node);
    void copyNode(const Node3D& source, const Node3D& copy);
    void setActive(const Node3D& node, bool value);

    NodeTree3D& tree;
    std::vector<Pool> pools;
};

} // namespace haylen::nodes3d
