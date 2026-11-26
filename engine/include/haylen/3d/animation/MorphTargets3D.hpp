#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/animation/FaceExpression3D.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::gltf {
struct Document;
}

namespace haylen::animation3d {

// The morph targets of a mesh: shapes that move its positions, normals and tangents by weights, such as the expressions of a face. Each target keeps only the vertices it moves, so a face of many targets that each move a few vertices takes little memory, and the weights of a mesh, which tracks of `weights` animate, blend the targets over its base shape.
class MorphTargets3D final {
  public:
    // The vertices a target moves, in increasing order, with the difference it adds to each of them at a weight of one. A list of differences is empty when the target does not move that attribute.
    struct Target {
        std::string name{};
        std::vector<std::uint32_t> vertices{};
        std::vector<math::Vec3> positions{};
        std::vector<math::Vec3> normals{};
        std::vector<math::Vec3> tangents{};
    };

    // The base shape in and the morphed shape out, with normals and tangents that may be empty. The tangents are their `x, y, z`, since morph targets leave the handedness of a tangent alone.
    struct Vertices {
        std::span<const math::Vec3> positions{};
        std::span<const math::Vec3> normals{};
        std::span<const math::Vec3> tangents{};
        std::span<math::Vec3> morphedPositions{};
        std::span<math::Vec3> morphedNormals{};
        std::span<math::Vec3> morphedTangents{};
    };

    // Throws `std::invalid_argument` for a target whose vertices are not in increasing order or past the vertices of the mesh, whose differences do not match its vertices or are not finite, and for two targets of the same name.
    MorphTargets3D(std::size_t vertexCount, std::vector<Target> targets);

    // Reads the morph targets of a primitive of a glTF mesh, keeping the vertices that each target moves, with the names that the `targetNames` of the `extras` of the mesh give them when it names every target. Throws `std::out_of_range` for a mesh or a primitive the document lacks and `std::invalid_argument` with a sentence that names the file for targets that break the rules.
    [[nodiscard]] static MorphTargets3D fromGltf(const gltf::Document& document, std::size_t mesh, std::size_t primitive);

    [[nodiscard]] std::size_t getVertexCount() const noexcept {
        return vertexCount;
    }
    [[nodiscard]] std::size_t getTargetCount() const noexcept {
        return targets.size();
    }
    [[nodiscard]] std::span<const Target> getTargets() const noexcept {
        return targets;
    }

    // The bytes that the differences of every target take.
    [[nodiscard]] std::size_t getMemorySize() const noexcept;

    [[nodiscard]] std::optional<std::size_t> findTarget(std::string_view name) const noexcept;

    // Finds the target of a face expression by the name of the expression.
    [[nodiscard]] std::optional<std::size_t> findTarget(FaceExpression3D expression) const noexcept;

    // Writes the base shape plus each target times its weight, with normals and tangents of a length of one again, where a normal or a tangent that the targets cancel stays zero. Weights may be negative or above one, as glTF allows. Allocates nothing. Throws `std::invalid_argument` for weights of another count than the targets or not finite, and spans of other sizes than the vertices.
    void apply(std::span<const float> weights, const Vertices& vertices) const;

  private:
    // The attributes of a glTF morph target that a target moves, in the order of the differences of `Target`.
    static constexpr std::array<std::string_view, 3> kAttributes{"POSITION", "NORMAL", "TANGENT"};

    [[noreturn]] void fail(std::size_t target, const std::string& problem) const;

    std::size_t vertexCount = 0;
    std::vector<Target> targets;
    std::map<std::string, std::size_t, std::less<>> names;
};

} // namespace haylen::animation3d
