#include "haylen/3d/nodes/ComponentType3D.hpp"

#include <algorithm>

namespace haylen::nodes3d {

bool ComponentType3D::runsIn(Phase phase) const noexcept {
    const std::span<const Phase> phases = getPhases();
    return std::ranges::find(phases, phase) != phases.end();
}

const ComponentType3D::Property* ComponentType3D::findProperty(std::string_view name) const noexcept {
    const std::span<const Property> properties = getProperties();
    const auto found = std::ranges::find(properties, name, &Property::name);
    return found == properties.end() ? nullptr : &*found;
}

} // namespace haylen::nodes3d
