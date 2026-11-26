#pragma once

#include <cstddef>
#include <span>

#include "haylen/3d/animation/SkinWeights3D.hpp"
#include "haylen/math/DualQuat.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::animation3d {

// Bends the vertices of a mesh by the palette of its skin on the CPU, the reference that tests hold the skinning of the GPU to and that tools use, such as for bounds and ray casts against a posed mesh. Linear blend skinning blends the matrices of the joints of each vertex by its weights, and dual quaternion skinning blends their rotations and translations, which keeps the volume of twisted joints.
class Skinning3D final {
  public:
    // The vertices in and out, with normals that may be empty. The results may be the inputs.
    struct Vertices {
        std::span<const math::Vec3> positions{};
        std::span<const math::Vec3> normals{};
        std::span<math::Vec3> skinnedPositions{};
        std::span<math::Vec3> skinnedNormals{};
    };

    // The vertices from which the work spreads over the job system.
    static constexpr std::size_t kParallelVertices = 16384;

    // Bends with linear blend skinning. A normal turns by the blended matrix and becomes a unit vector again, while a normal that a joint scaled to zero folds stays zero. Allocates nothing without a job system. Throws `std::invalid_argument` for spans of other sizes than the vertices of the weights and a palette of another size than their joints.
    static void applyLinear(const SkinWeights3D& weights, std::span<const math::Mat4> palette, const Vertices& vertices, core::JobSystem* jobs = nullptr);

    // Bends with dual quaternion skinning, and throws as `applyLinear` does.
    static void applyDualQuat(const SkinWeights3D& weights, std::span<const math::DualQuat> palette, const Vertices& vertices, core::JobSystem* jobs = nullptr);

  private:
    static constexpr std::size_t kParallelGrain = 4096;

    static void check(const SkinWeights3D& weights, std::size_t paletteSize, const Vertices& vertices);

    // Runs the body over the range of vertices, in chunks on the job system for a large mesh.
    template <typename Body> static void run(std::size_t count, core::JobSystem* jobs, const Body& body);
};

} // namespace haylen::animation3d
