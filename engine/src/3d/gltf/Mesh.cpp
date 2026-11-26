#include "haylen/3d/gltf/Mesh.hpp"

#include <algorithm>

namespace haylen::gltf {

std::optional<std::size_t> Mesh::Primitive::findAttribute(std::string_view attribute) const noexcept {
    const auto found = std::ranges::lower_bound(attributes, attribute, {}, &Attribute::name);
    if (found == attributes.end() || found->name != attribute) {
        return std::nullopt;
    }

    return found->accessor;
}

std::size_t Mesh::getTargetCount() const noexcept {
    for (const Primitive& primitive : primitives) {
        if (!primitive.targets.empty()) {
            return primitive.targets.size();
        }
    }

    return 0;
}

} // namespace haylen::gltf
