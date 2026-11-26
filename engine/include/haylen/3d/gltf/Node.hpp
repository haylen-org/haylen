#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::gltf {

// A place in the hierarchy of a scene, with its transform relative to its parent and what it holds: a mesh, with the skin that bends it, a camera and its children. A node gives its transform either as a matrix or as translation, rotation and scale, and the reader fills both forms, decomposing a matrix.
struct Node {
    std::string name;
    std::vector<std::size_t> children;

    // The one node whose children list this node, which the reader finds.
    std::optional<std::size_t> parent;
    std::optional<std::size_t> camera;
    std::optional<std::size_t> mesh;
    std::optional<std::size_t> skin;

    // The matrix as the file gives it, which animations never target.
    std::optional<math::Mat4> matrix;
    math::Vec3 translation{0.0F, 0.0F, 0.0F};
    math::Quat rotation;
    math::Vec3 scale{1.0F, 1.0F, 1.0F};

    // The morph weights that replace the default weights of the mesh, empty when the node keeps them.
    std::vector<float> weights;
    core::Json extras;
    core::Json extensions;

    // The transform relative to the parent: the matrix of the file, or the composition of translation, rotation and scale.
    [[nodiscard]] math::Mat4 getLocalMatrix() const noexcept;
};

} // namespace haylen::gltf
