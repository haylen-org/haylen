#include "3d/animation/GltfNodeNames3D.hpp"

#include <format>
#include <string_view>
#include <unordered_map>

namespace haylen::animation3d {

std::vector<std::string> GltfNodeNames3D::of(const gltf::Document& document) {
    std::unordered_map<std::string_view, std::size_t> uses;
    for (const gltf::Node& node : document.nodes) {
        ++uses[node.name];
    }

    std::vector<std::string> names;
    names.reserve(document.nodes.size());
    for (std::size_t index = 0; index < document.nodes.size(); ++index) {
        const std::string& name = document.nodes[index].name;
        const bool unique = !name.empty() && name.front() != '/' && uses[name] == 1;
        names.push_back(unique ? name : std::format("/nodes/{}", index));
    }

    return names;
}

} // namespace haylen::animation3d
