#include "haylen/3d/animation/Skeleton3D.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>
#include <utility>

#include "3d/animation/GltfNodeNames3D.hpp"
#include "haylen/3d/gltf/Document.hpp"

namespace haylen::animation3d {

Skeleton3D::Skeleton3D(std::vector<Bone> skeletonBones) : bones(std::move(skeletonBones)) {
    if (bones.empty() || bones.size() > kMaxBones) {
        throw std::invalid_argument(std::format("A skeleton holds from 1 to {} bones, while it was given {}.", kMaxBones, bones.size()));
    }

    for (std::size_t index = 0; index < bones.size(); ++index) {
        const Bone& bone = bones[index];
        if (bone.name.empty() || !indices.emplace(bone.name, index).second) {
            throw std::invalid_argument(std::format("Every bone of a skeleton needs a name of its own, while the name \"{}\" of bone {} is empty or taken.", bone.name, index));
        }

        if (bone.parent && *bone.parent >= index) {
            throw std::invalid_argument(std::format("The bone \"{}\" names the parent {}, while the parent of a bone comes before it.", bone.name, *bone.parent));
        }

        const math::Quat& rotation = bone.rest.rotation;
        const bool finite = bone.rest.position.isFinite() && bone.rest.scale.isFinite() && std::isfinite(rotation.x) && std::isfinite(rotation.y) && std::isfinite(rotation.z) && std::isfinite(rotation.w);
        if (!finite) {
            throw std::invalid_argument(std::format("The rest pose of the bone \"{}\" holds a number that is not finite.", bone.name));
        }
    }
}

// The bones are the joints and every node above them, which come in the order of their depth, so a parent always comes before its children.
Skeleton3D Skeleton3D::fromGltf(const gltf::Document& document, std::size_t skin) {
    if (skin >= document.skins.size()) {
        throw std::out_of_range(std::format("The glTF file \"{}\" has {} skins, so it has no skin {}.", document.file, document.skins.size(), skin));
    }

    const std::size_t count = document.nodes.size();
    std::vector<std::size_t> depths(count, 0);
    std::vector<bool> included(count, false);
    for (const std::size_t joint : document.skins[skin].joints) {
        if (joint >= count) {
            throw std::invalid_argument(std::format("The skin {} of the glTF file \"{}\" names node {}, which the file lacks. Export the model again from the tool that made it, so it follows glTF 2.0.", skin, document.file, joint));
        }

        // A walk up the hierarchy longer than its nodes would go around a cycle.
        std::vector<std::size_t> path;
        for (std::optional<std::size_t> node = joint; node; node = document.nodes[*node].parent) {
            if (*node >= count || path.size() == count) {
                throw std::invalid_argument(std::format("The nodes above node {} of the glTF file \"{}\" do not form a tree. Export the model again from the tool that made it, so it follows glTF 2.0.", joint, document.file));
            }

            path.push_back(*node);
        }

        for (std::size_t step = 0; step < path.size(); ++step) {
            included[path[step]] = true;
            depths[path[step]] = path.size() - 1 - step;
        }
    }

    std::vector<std::size_t> order;
    for (std::size_t node = 0; node < count; ++node) {
        if (included[node]) {
            order.push_back(node);
        }
    }

    std::ranges::stable_sort(order, {}, [&depths](std::size_t node) { return depths[node]; });
    std::vector<std::size_t> boneOfNode(count, 0);
    for (std::size_t bone = 0; bone < order.size(); ++bone) {
        boneOfNode[order[bone]] = bone;
    }

    const std::vector<std::string> names = GltfNodeNames3D::of(document);
    std::vector<Bone> skeletonBones;
    skeletonBones.reserve(order.size());
    for (const std::size_t node : order) {
        const gltf::Node& source = document.nodes[node];
        const std::optional<std::size_t> parent = source.parent ? std::optional(boneOfNode[*source.parent]) : std::nullopt;
        skeletonBones.push_back({.name = names[node], .parent = parent, .rest = {.position = source.translation, .rotation = source.rotation, .scale = source.scale}});
    }

    try {
        return Skeleton3D(std::move(skeletonBones));
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::format("The skin {} of the glTF file \"{}\" does not make a skeleton. {}", skin, document.file, error.what()));
    }
}

std::optional<std::size_t> Skeleton3D::findBone(std::string_view name) const noexcept {
    const auto found = indices.find(name);
    return found != indices.end() ? std::optional(found->second) : std::nullopt;
}

void Skeleton3D::computeModelMatrices(std::span<const math::Transform3D> local, std::span<math::Mat4> result) const {
    if (local.size() != bones.size() || result.size() != bones.size()) {
        throw std::invalid_argument(std::format("The model matrices of a skeleton of {} bones take {} local transforms and matrices, while {} and {} were given.", bones.size(), bones.size(), local.size(), result.size()));
    }

    for (std::size_t bone = 0; bone < bones.size(); ++bone) {
        const math::Mat4 matrix = local[bone].toMatrix();
        result[bone] = bones[bone].parent ? result[*bones[bone].parent] * matrix : matrix;
    }
}

} // namespace haylen::animation3d
