#include "haylen/2d/nodes/Prefab.hpp"

#include <stdexcept>
#include <utility>

namespace haylen::nodes2d {

Prefab Prefab::parse(core::Json value) {
    NodeSerializer::validate(value);
    Prefab prefab;
    prefab.description = std::move(value);
    return prefab;
}

Node Prefab::instantiate(NodeTree& tree, const Node& parent, const core::Json& overrides, const NodeSerializer::Assets& assets) const {
    if (!overrides.is_object()) {
        throw std::invalid_argument("The overrides of a prefab must be an object of the keys of a description.");
    }
    if (overrides.empty()) {
        return NodeSerializer::load(tree, parent, description, assets);
    }
    core::Json root = description;
    for (const auto& [key, value] : overrides.items()) {
        root[key] = value;
    }
    return NodeSerializer::load(tree, parent, root, assets);
}

} // namespace haylen::nodes2d
