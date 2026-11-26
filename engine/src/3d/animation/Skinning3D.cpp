#include "haylen/3d/animation/Skinning3D.hpp"

#include <array>
#include <format>
#include <stdexcept>

#include "3d/animation/VertexHelper3D.hpp"
#include "haylen/core/JobSystem.hpp"

namespace haylen::animation3d {

void Skinning3D::check(const SkinWeights3D& weights, std::size_t paletteSize, const Vertices& vertices) {
    const std::size_t count = weights.getVertexCount();
    if (paletteSize != weights.getJointCount()) {
        throw std::invalid_argument(std::format("Skin weights of {} joints take a palette of {} joints, while it holds {}.", weights.getJointCount(), weights.getJointCount(), paletteSize));
    }

    if (vertices.positions.size() != count || vertices.skinnedPositions.size() != count) {
        throw std::invalid_argument(std::format("Skin weights of {} vertices bend {} positions into {}, while {} and {} were given.", count, count, count, vertices.positions.size(), vertices.skinnedPositions.size()));
    }

    if (vertices.normals.size() != vertices.skinnedNormals.size() || (!vertices.normals.empty() && vertices.normals.size() != count)) {
        throw std::invalid_argument(std::format("Skin weights of {} vertices bend no normals or {} normals into as many, while {} and {} were given.", count, count, vertices.normals.size(), vertices.skinnedNormals.size()));
    }
}

template <typename Body> void Skinning3D::run(std::size_t count, core::JobSystem* jobs, const Body& body) {
    if (jobs != nullptr && count >= kParallelVertices) {
        jobs->parallelFor(0, count, kParallelGrain, body);
        return;
    }

    body(0, count);
}

void Skinning3D::applyLinear(const SkinWeights3D& weights, std::span<const math::Mat4> palette, const Vertices& vertices, core::JobSystem* jobs) {
    check(weights, palette.size(), vertices);
    const std::size_t influences = weights.getInfluenceCount();
    const std::span<const std::uint16_t> joints = weights.getJoints();
    const std::span<const float> amounts = weights.getWeights();

    // clang-format off
    run(weights.getVertexCount(), jobs, [&](std::size_t begin, std::size_t end) {
        for (std::size_t vertex = begin; vertex < end; ++vertex) {
            math::Mat4 blended;
            blended.values.fill(0.0F);
            for (std::size_t place = 0; place < influences; ++place) {
                const float weight = amounts[vertex * influences + place];
                const math::Mat4& joint = palette[joints[vertex * influences + place]];
                for (std::size_t element = 0; element < 16; ++element) {
                    blended.values[element] += joint.values[element] * weight;
                }
            }

            const math::Vec3 position = vertices.positions[vertex];
            vertices.skinnedPositions[vertex] = blended.apply(position);
            if (!vertices.normals.empty()) {
                const math::Vec3 normal = vertices.normals[vertex];
                vertices.skinnedNormals[vertex] = VertexHelper3D::unitOrZero(blended.applyVector(normal));
            }
        }
    });
    // clang-format on
}

void Skinning3D::applyDualQuat(const SkinWeights3D& weights, std::span<const math::DualQuat> palette, const Vertices& vertices, core::JobSystem* jobs) {
    check(weights, palette.size(), vertices);
    const std::size_t influences = weights.getInfluenceCount();
    const std::span<const std::uint16_t> joints = weights.getJoints();
    const std::span<const float> amounts = weights.getWeights();

    // clang-format off
    run(weights.getVertexCount(), jobs, [&](std::size_t begin, std::size_t end) {
        std::array<math::DualQuat, 8> parts{};
        std::array<float, 8> partWeights{};
        for (std::size_t vertex = begin; vertex < end; ++vertex) {
            std::size_t count = 0;
            for (std::size_t place = 0; place < influences; ++place) {
                const float weight = amounts[vertex * influences + place];
                if (weight > 0.0F) {
                    parts[count] = palette[joints[vertex * influences + place]];
                    partWeights[count] = weight;
                    ++count;
                }
            }

            const math::DualQuat blended = math::DualQuat::blend(std::span(parts).first(count), std::span(partWeights).first(count));
            const math::Vec3 position = vertices.positions[vertex];
            vertices.skinnedPositions[vertex] = blended.apply(position);
            if (!vertices.normals.empty()) {
                const math::Vec3 normal = vertices.normals[vertex];
                vertices.skinnedNormals[vertex] = VertexHelper3D::unitOrZero(blended.getRotation().rotate(normal));
            }
        }
    });
    // clang-format on
}

} // namespace haylen::animation3d
