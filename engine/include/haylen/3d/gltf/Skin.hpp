#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::gltf {

// The joints that bend a skinned mesh, with the matrices that bring each vertex from the space of the mesh into the space of each joint. A skin without inverse bind matrices takes identities.
struct Skin {
    std::string name;
    std::optional<std::size_t> inverseBindMatrices;

    // The root of the hierarchy of the joints, or their common ancestor.
    std::optional<std::size_t> skeleton;
    std::vector<std::size_t> joints;
    core::Json extras;
    core::Json extensions;
};

} // namespace haylen::gltf
