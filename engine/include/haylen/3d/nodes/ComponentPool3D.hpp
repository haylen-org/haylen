#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "haylen/3d/nodes/ComponentType3D.hpp"
#include "haylen/3d/nodes/Node3D.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::nodes3d {

// The components of one type in one tree, which the pool keeps in arrays by a dense index that the component store hands it: the store adds at the end and removes by moving the last component into the place that frees. A pool runs no Lua and never changes the components of the tree while it runs a phase.
class ComponentPool3D {
  public:
    // The components that a phase runs: the node of each component and whether that node is on in the hierarchy, by the index of the component.
    struct Batch {
        std::span<const Node3D> nodes{};
        std::span<const std::uint8_t> active{};
        float deltaSeconds = 0.0F;
    };

    ComponentPool3D() = default;
    virtual ~ComponentPool3D() = default;

    ComponentPool3D(const ComponentPool3D&) = delete;
    ComponentPool3D& operator=(const ComponentPool3D&) = delete;

    // Adds the component of a node after the components of the pool, from options whose keys are properties of the type. Throws `std::invalid_argument` for an unknown key or a value of the wrong kind, before it adds anything.
    virtual void add(const Node3D& node, const core::Json& options) = 0;

    // Removes the component at the index, whose place the last component takes.
    virtual void remove(std::size_t index) = 0;

    // Reads and writes a property of the component at the index, with the errors of `add` for a name or a value it refuses.
    [[nodiscard]] virtual core::Json get(std::size_t index, std::string_view property) const = 0;
    virtual void set(std::size_t index, std::string_view property, const core::Json& value) = 0;

    // Returns the options that make the component again, which a clone and saving use.
    [[nodiscard]] virtual core::Json save(std::size_t index) const = 0;

    // Runs every component of the batch in a phase that the type names. A component whose node is off rests.
    virtual void run(ComponentType3D::Phase phase, const Batch& batch) = 0;
};

} // namespace haylen::nodes3d
