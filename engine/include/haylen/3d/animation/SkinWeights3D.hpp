#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace haylen::gltf {
struct Document;
}

namespace haylen::animation3d {

// The joints and weights of the vertices of a skinned mesh, packed with the same number of influences for every vertex, 4 or 8, as a vertex shader reads them: the joints of each vertex as indices into the joints of its skin and their weights, which add up to one.
class SkinWeights3D final {
  public:
    // How far the weights of a vertex may add up from one.
    static constexpr float kSumTolerance = 1e-3F;

    // Throws `std::invalid_argument` for influences other than 4 or 8, lists whose sizes do not match, a joint past `jointCount`, a joint named twice with weight by one vertex, and weights that are negative, not finite or that do not add up to one.
    SkinWeights3D(std::size_t influences, std::vector<std::uint16_t> joints, std::vector<float> weights, std::size_t jointCount);

    // Packs the joints and weights of a primitive of a glTF mesh, every set of `JOINTS_n` and `WEIGHTS_n` together: each vertex keeps its `influences` largest weights, in the order of their size, which add up to one again, and the rest of its places hold joint 0 with a weight of zero. Throws `std::out_of_range` for a mesh or a primitive the document lacks and `std::invalid_argument` with a sentence that names the file and the vertex for joints and weights that break the rules.
    [[nodiscard]] static SkinWeights3D fromGltf(const gltf::Document& document, std::size_t mesh, std::size_t primitive, std::size_t jointCount, std::size_t influences);

    [[nodiscard]] std::size_t getInfluenceCount() const noexcept {
        return influences;
    }
    [[nodiscard]] std::size_t getVertexCount() const noexcept {
        return influences == 0 ? 0 : weights.size() / influences;
    }
    [[nodiscard]] std::size_t getJointCount() const noexcept {
        return jointCount;
    }
    [[nodiscard]] std::span<const std::uint16_t> getJoints() const noexcept {
        return joints;
    }
    [[nodiscard]] std::span<const float> getWeights() const noexcept {
        return weights;
    }

  private:
    std::size_t influences = 4;
    std::vector<std::uint16_t> joints;
    std::vector<float> weights;
    std::size_t jointCount = 0;
};

} // namespace haylen::animation3d
