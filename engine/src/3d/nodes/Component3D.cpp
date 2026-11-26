#include "haylen/3d/nodes/Component3D.hpp"

#include "haylen/3d/nodes/NodeTree3D.hpp"

namespace haylen::nodes3d {

bool Component3D::isValid() const noexcept {
    return type && node.isValid() && node.getTree().getComponents().has(node, *type);
}

core::Json Component3D::get(std::string_view property) const {
    return node.getTree().getComponents().get(node, *type, property);
}

void Component3D::set(std::string_view property, const core::Json& value) {
    node.getTree().getComponents().set(node, *type, property, value);
}

core::Json Component3D::save() const {
    return node.getTree().getComponents().save(node, *type);
}

void Component3D::remove() {
    node.getTree().getComponents().remove(node, *type);
}

} // namespace haylen::nodes3d
