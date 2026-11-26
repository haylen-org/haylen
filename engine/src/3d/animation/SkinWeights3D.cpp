#include "haylen/3d/animation/SkinWeights3D.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/3d/animation/Skeleton3D.hpp"
#include "haylen/3d/gltf/AccessorReader.hpp"
#include "haylen/3d/gltf/Document.hpp"

namespace haylen::animation3d {

SkinWeights3D::SkinWeights3D(std::size_t vertexInfluences, std::vector<std::uint16_t> vertexJoints, std::vector<float> vertexWeights, std::size_t skinJoints) : influences(vertexInfluences), joints(std::move(vertexJoints)), weights(std::move(vertexWeights)), jointCount(skinJoints) {
    if (influences != 4 && influences != 8) {
        throw std::invalid_argument(std::format("A vertex takes 4 or 8 influences, while {} were asked for.", influences));
    }

    if (joints.size() != weights.size() || weights.size() % influences != 0) {
        throw std::invalid_argument(std::format("The skin weights hold {} joints and {} weights, while each of their vertices takes {} of both.", joints.size(), weights.size(), influences));
    }

    const std::size_t vertices = weights.size() / influences;
    for (std::size_t vertex = 0; vertex < vertices; ++vertex) {
        float sum = 0.0F;
        for (std::size_t place = 0; place < influences; ++place) {
            const std::size_t index = vertex * influences + place;
            const float weight = weights[index];
            if (!std::isfinite(weight) || weight < 0.0F) {
                throw std::invalid_argument(std::format("Vertex {} of the skin weights has the weight {}, while weights are finite and zero or more.", vertex, weight));
            }

            if (joints[index] >= jointCount) {
                throw std::invalid_argument(std::format("Vertex {} of the skin weights names joint {} of a skin with {} joints.", vertex, joints[index], jointCount));
            }

            const bool twice = weight > 0.0F && std::ranges::any_of(std::span(joints).subspan(vertex * influences, place), [&](std::uint16_t joint) { return joint == joints[index]; });
            if (twice) {
                throw std::invalid_argument(std::format("Vertex {} of the skin weights names joint {} twice.", vertex, joints[index]));
            }

            sum += weight;
        }

        if (std::abs(sum - 1.0F) > kSumTolerance) {
            throw std::invalid_argument(std::format("The weights of vertex {} of the skin weights add up to {} and not to 1.", vertex, sum));
        }
    }
}

// Each vertex gathers the joints of every set with a weight, keeps the largest, the joint of the lower index first between equal weights, and spreads their sum back to one.
SkinWeights3D SkinWeights3D::fromGltf(const gltf::Document& document, std::size_t mesh, std::size_t primitive, std::size_t jointCount, std::size_t influences) {
    const gltf::Mesh::Primitive& source = document.meshes.at(mesh).primitives.at(primitive);
    const std::string owner = std::format("primitive {} of mesh {} of the glTF file \"{}\"", primitive, mesh, document.file);
    if (influences != 4 && influences != 8) {
        throw std::invalid_argument(std::format("A vertex takes 4 or 8 influences, while {} were asked for.", influences));
    }

    if (jointCount == 0 || jointCount > Skeleton3D::kMaxBones) {
        throw std::invalid_argument(std::format("A skin holds from 1 to {} joints, while it was given {}.", Skeleton3D::kMaxBones, jointCount));
    }

    std::vector<std::vector<std::uint32_t>> jointSets;
    std::vector<std::vector<float>> weightSets;
    for (std::size_t set = 0;; ++set) {
        const std::optional<std::size_t> jointAccessor = source.findAttribute("JOINTS_" + std::to_string(set));
        const std::optional<std::size_t> weightAccessor = source.findAttribute("WEIGHTS_" + std::to_string(set));
        if (!jointAccessor || !weightAccessor) {
            break;
        }

        jointSets.push_back(gltf::AccessorReader::readUnsigned(document, *jointAccessor));
        weightSets.push_back(gltf::AccessorReader::readFloats(document, *weightAccessor));
        if (jointSets.back().size() != jointSets.front().size() || weightSets.back().size() != jointSets.front().size() || jointSets.back().size() % 4 != 0) {
            throw std::invalid_argument(std::format("The joints and weights of {} hold different numbers of vertices. Export the model again from the tool that made it, so it follows glTF 2.0.", owner));
        }
    }

    if (jointSets.empty()) {
        throw std::invalid_argument(std::format("The {} has no joints and weights, so a skin cannot bend it.", owner));
    }

    const std::size_t vertices = jointSets.front().size() / 4;
    std::vector<std::uint16_t> packedJoints(vertices * influences, 0);
    std::vector<float> packedWeights(vertices * influences, 0.0F);
    std::vector<std::pair<float, std::uint32_t>> found;
    for (std::size_t vertex = 0; vertex < vertices; ++vertex) {
        found.clear();
        for (std::size_t set = 0; set < jointSets.size(); ++set) {
            for (std::size_t component = 0; component < 4; ++component) {
                const float weight = weightSets[set][vertex * 4 + component];
                const std::uint32_t joint = jointSets[set][vertex * 4 + component];
                if (!std::isfinite(weight) || weight < 0.0F || joint >= jointCount) {
                    throw std::invalid_argument(std::format("Vertex {} of {} names joint {} with the weight {} of a skin with {} joints. Export the model again from the tool that made it, so it follows glTF 2.0.", vertex, owner, joint, weight, jointCount));
                }

                if (weight > 0.0F) {
                    found.emplace_back(weight, joint);
                }
            }
        }

        std::ranges::sort(found, [](const auto& lhs, const auto& rhs) { return lhs.first != rhs.first ? lhs.first > rhs.first : lhs.second < rhs.second; });
        const std::size_t kept = std::min(found.size(), influences);
        float sum = 0.0F;
        for (std::size_t place = 0; place < kept; ++place) {
            sum += found[place].first;
        }

        if (sum == 0.0F) {
            throw std::invalid_argument(std::format("Vertex {} of {} has no weight, while the weights of a vertex add up to one. Export the model again from the tool that made it, so it follows glTF 2.0.", vertex, owner));
        }

        for (std::size_t place = 0; place < kept; ++place) {
            packedJoints[vertex * influences + place] = static_cast<std::uint16_t>(found[place].second);
            packedWeights[vertex * influences + place] = found[place].first / sum;
        }
    }

    try {
        return SkinWeights3D(influences, std::move(packedJoints), std::move(packedWeights), jointCount);
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::format("The joints and weights of {} do not make skin weights. {}", owner, error.what()));
    }
}

} // namespace haylen::animation3d
