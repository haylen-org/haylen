#include "haylen/3d/nodes/ComponentRegistry3D.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace haylen::nodes3d {

void ComponentRegistry3D::add(std::shared_ptr<const ComponentType3D> type) {
    if (!type || type->getName().empty()) {
        throw std::invalid_argument("A component type needs a name.");
    }

    if (find(type->getName())) {
        throw std::invalid_argument("A component type named \"" + std::string(type->getName()) + "\" is registered already.");
    }

    types.push_back(std::move(type));
}

std::shared_ptr<const ComponentType3D> ComponentRegistry3D::find(std::string_view name) const {
    for (const std::shared_ptr<const ComponentType3D>& type : types) {
        if (type->getName() == name) {
            return type;
        }
    }

    return nullptr;
}

} // namespace haylen::nodes3d
