#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::gltf {

// A set of root nodes that show together. Several scenes may share nodes.
struct Scene {
    std::string name;
    std::vector<std::size_t> nodes;
    core::Json extras;
    core::Json extensions;
};

} // namespace haylen::gltf
