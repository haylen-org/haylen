#include "haylen/3d/animation/MorphTargets3D.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <stdexcept>
#include <utility>

#include "3d/animation/VertexHelper3D.hpp"
#include "haylen/3d/gltf/AccessorReader.hpp"
#include "haylen/3d/gltf/Document.hpp"

namespace haylen::animation3d {

MorphTargets3D::MorphTargets3D(std::size_t meshVertices, std::vector<Target> meshTargets) : vertexCount(meshVertices), targets(std::move(meshTargets)) {
    for (std::size_t index = 0; index < targets.size(); ++index) {
        const Target& target = targets[index];
        if (!target.name.empty() && !names.emplace(target.name, index).second) {
            fail(index, "has the name of another target, while each target needs a name of its own");
        }

        for (std::size_t place = 0; place < target.vertices.size(); ++place) {
            const std::uint32_t vertex = target.vertices[place];
            if (vertex >= vertexCount || (place > 0 && vertex <= target.vertices[place - 1])) {
                fail(index, std::format("names vertex {} at place {}, while the vertices of a target grow and stay below the {} vertices of the mesh", vertex, place, vertexCount));
            }
        }

        for (const std::vector<math::Vec3>* differences : {&target.positions, &target.normals, &target.tangents}) {
            if (!differences->empty() && differences->size() != target.vertices.size()) {
                fail(index, std::format("holds {} differences of an attribute for its {} vertices", differences->size(), target.vertices.size()));
            }

            if (!std::ranges::all_of(*differences, &math::Vec3::isFinite)) {
                fail(index, "holds a difference that is not finite");
            }
        }
    }
}

void MorphTargets3D::fail(std::size_t target, const std::string& problem) const {
    const std::string& name = targets[target].name;
    throw std::invalid_argument(name.empty() ? std::format("Morph target {} {}.", target, problem) : std::format("The morph target \"{}\" {}.", name, problem));
}

// Each target keeps the vertices that any of its attributes moves, so a target that moves the corner of a mouth holds that corner alone.
MorphTargets3D MorphTargets3D::fromGltf(const gltf::Document& document, std::size_t mesh, std::size_t primitive) {
    const gltf::Mesh& source = document.meshes.at(mesh);
    const gltf::Mesh::Primitive& shape = source.primitives.at(primitive);
    const std::string owner = std::format("primitive {} of mesh {} of the glTF file \"{}\"", primitive, mesh, document.file);
    // Every attribute of a primitive holds one value for each vertex, so the first of them counts the vertices.
    if (shape.attributes.empty() || shape.attributes.front().accessor >= document.accessors.size()) {
        throw std::invalid_argument(std::format("The {} has no attributes for its morph targets to move. Export the model again from the tool that made it, so it follows glTF 2.0.", owner));
    }

    const auto vertices = static_cast<std::size_t>(document.accessors[shape.attributes.front().accessor].count);
    std::vector<std::string> targetNames(shape.targets.size());
    const auto extrasNames = source.extras.is_object() ? source.extras.find("targetNames") : source.extras.end();
    if (extrasNames != source.extras.end() && extrasNames->is_array() && extrasNames->size() == shape.targets.size() && std::ranges::all_of(*extrasNames, [](const core::Json& name) { return name.is_string(); })) {
        for (std::size_t target = 0; target < targetNames.size(); ++target) {
            targetNames[target] = (*extrasNames)[target].get<std::string>();
        }
    }

    std::vector<Target> readTargets;
    readTargets.reserve(shape.targets.size());
    for (std::size_t index = 0; index < shape.targets.size(); ++index) {
        std::vector<std::vector<float>> attributes(kAttributes.size());
        for (std::size_t attribute = 0; attribute < kAttributes.size(); ++attribute) {
            const auto found = std::ranges::find(shape.targets[index], kAttributes[attribute], &gltf::Mesh::Attribute::name);
            if (found == shape.targets[index].end()) {
                continue;
            }

            if (found->accessor >= document.accessors.size() || document.accessors[found->accessor].type != gltf::Accessor::Type::Vec3) {
                throw std::invalid_argument(std::format("Morph target {} of {} moves \"{}\" by an accessor that is not of 3D vectors. Export the model again from the tool that made it, so it follows glTF 2.0.", index, owner, kAttributes[attribute]));
            }

            attributes[attribute] = gltf::AccessorReader::readFloats(document, found->accessor);
            if (attributes[attribute].size() != vertices * 3) {
                throw std::invalid_argument(std::format("Morph target {} of {} moves \"{}\" of a number of vertices other than the primitive's. Export the model again from the tool that made it, so it follows glTF 2.0.", index, owner, kAttributes[attribute]));
            }
        }

        Target target{.name = targetNames[index]};
        const std::array<std::vector<math::Vec3>*, 3> lists{&target.positions, &target.normals, &target.tangents};
        const bool movesNothing = std::ranges::all_of(attributes, [](const std::vector<float>& values) { return values.empty(); });
        for (std::size_t vertex = 0; !movesNothing && vertex < vertices; ++vertex) {
            const bool moves = std::ranges::any_of(attributes, [vertex](const std::vector<float>& values) { return !values.empty() && (values[vertex * 3] != 0.0F || values[vertex * 3 + 1] != 0.0F || values[vertex * 3 + 2] != 0.0F); });
            if (!moves) {
                continue;
            }

            target.vertices.push_back(static_cast<std::uint32_t>(vertex));
            for (std::size_t attribute = 0; attribute < kAttributes.size(); ++attribute) {
                const std::vector<float>& values = attributes[attribute];
                if (!values.empty()) {
                    lists[attribute]->push_back({values[vertex * 3], values[vertex * 3 + 1], values[vertex * 3 + 2]});
                }
            }
        }

        readTargets.push_back(std::move(target));
    }

    try {
        return MorphTargets3D(vertices, std::move(readTargets));
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::format("The morph targets of {} do not make morph targets. {}", owner, error.what()));
    }
}

std::size_t MorphTargets3D::getMemorySize() const noexcept {
    std::size_t size = 0;
    for (const Target& target : targets) {
        size += target.vertices.size() * sizeof(std::uint32_t) + (target.positions.size() + target.normals.size() + target.tangents.size()) * sizeof(math::Vec3);
    }

    return size;
}

std::optional<std::size_t> MorphTargets3D::findTarget(std::string_view name) const noexcept {
    const auto found = names.find(name);
    return found != names.end() ? std::optional(found->second) : std::nullopt;
}

std::optional<std::size_t> MorphTargets3D::findTarget(FaceExpression3D expression) const noexcept {
    return findTarget(core::EnumNames<FaceExpression3D>::name(expression));
}

void MorphTargets3D::apply(std::span<const float> weights, const Vertices& vertices) const {
    if (weights.size() != targets.size() || !std::ranges::all_of(weights, [](float weight) { return std::isfinite(weight); })) {
        throw std::invalid_argument(std::format("The morph targets take {} finite weights, one for each target, while {} weights were given.", targets.size(), weights.size()));
    }

    const bool normals = !vertices.normals.empty();
    const bool tangents = !vertices.tangents.empty();
    const bool sizes = vertices.positions.size() == vertexCount && vertices.morphedPositions.size() == vertexCount && (!normals || (vertices.normals.size() == vertexCount && vertices.morphedNormals.size() == vertexCount)) && (!tangents || (vertices.tangents.size() == vertexCount && vertices.morphedTangents.size() == vertexCount));
    if (!sizes) {
        throw std::invalid_argument(std::format("The morph targets move {} vertices, while the positions, normals and tangents in and out hold other numbers of them.", vertexCount));
    }

    std::ranges::copy(vertices.positions, vertices.morphedPositions.begin());
    if (normals) {
        std::ranges::copy(vertices.normals, vertices.morphedNormals.begin());
    }

    if (tangents) {
        std::ranges::copy(vertices.tangents, vertices.morphedTangents.begin());
    }

    for (std::size_t index = 0; index < targets.size(); ++index) {
        const float weight = weights[index];
        const Target& target = targets[index];
        if (weight == 0.0F) {
            continue;
        }

        for (std::size_t place = 0; place < target.vertices.size(); ++place) {
            const std::uint32_t vertex = target.vertices[place];
            if (!target.positions.empty()) {
                vertices.morphedPositions[vertex] += target.positions[place] * weight;
            }

            if (normals && !target.normals.empty()) {
                vertices.morphedNormals[vertex] += target.normals[place] * weight;
            }

            if (tangents && !target.tangents.empty()) {
                vertices.morphedTangents[vertex] += target.tangents[place] * weight;
            }
        }
    }

    for (std::size_t vertex = 0; normals && vertex < vertexCount; ++vertex) {
        vertices.morphedNormals[vertex] = VertexHelper3D::unitOrZero(vertices.morphedNormals[vertex]);
    }

    for (std::size_t vertex = 0; tangents && vertex < vertexCount; ++vertex) {
        vertices.morphedTangents[vertex] = VertexHelper3D::unitOrZero(vertices.morphedTangents[vertex]);
    }
}

} // namespace haylen::animation3d
