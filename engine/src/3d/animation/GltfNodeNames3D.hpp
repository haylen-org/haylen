#pragma once

#include <string>
#include <vector>

#include "haylen/3d/gltf/Document.hpp"

namespace haylen::animation3d {

// Names the nodes of a glTF document for the tracks of clips: a node keeps its name when no other node of the file takes it, and takes its glTF pointer, such as `/nodes/3`, when it has no name, shares its name or has a name that starts with a slash, which only pointers do.
class GltfNodeNames3D final {
  public:
    [[nodiscard]] static std::vector<std::string> of(const gltf::Document& document);
};

} // namespace haylen::animation3d
