#pragma once

#include "haylen/2d/nodes/Node.hpp"
#include "haylen/2d/nodes/NodeSerializer.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::nodes2d {

class NodeTree;

// The checked description of a subtree, such as a `.prefab` file, which a tree instantiates as often as it needs.
class Prefab final {
  public:
    // Throws `std::invalid_argument` for a description that `NodeSerializer::validate` refuses.
    [[nodiscard]] static Prefab parse(core::Json value);

    [[nodiscard]] const core::Json& getDescription() const noexcept {
        return description;
    }

    // Builds the subtree under the parent, or at the root for an invalid parent, where the keys of the overrides replace the keys of the root description, such as its `x` and `y`.
    Node instantiate(NodeTree& tree, const Node& parent, const core::Json& overrides, const NodeSerializer::Assets& assets) const;

  private:
    core::Json description;
};

} // namespace haylen::nodes2d
