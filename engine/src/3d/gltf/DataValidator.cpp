#include "3d/gltf/DataValidator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>

#include "3d/gltf/AccessorView.hpp"
#include "3d/gltf/FormatNames.hpp"
#include "haylen/3d/gltf/AccessorReader.hpp"

namespace haylen::gltf {

using Problem = ReadError::Problem;

DataValidator::DataValidator(const Document& target, const Diagnostics& documentDiagnostics) : document(target), diagnostics(documentDiagnostics), checks(target.accessors.size()) {}

void DataValidator::validate(const Document& document, const Diagnostics& diagnostics) {
    DataValidator validator(document, diagnostics);
    validator.collect();
    for (std::size_t accessor = 0; accessor < document.accessors.size(); ++accessor) {
        validator.validateSparse(accessor);
    }

    for (std::size_t accessor = 0; accessor < document.accessors.size(); ++accessor) {
        validator.validateAccessor(accessor);
    }

    validator.validateSkinning();
}

void DataValidator::collect() {
    for (std::size_t meshIndex = 0; meshIndex < document.meshes.size(); ++meshIndex) {
        const Mesh& mesh = document.meshes[meshIndex];
        for (std::size_t index = 0; index < mesh.primitives.size(); ++index) {
            const Mesh::Primitive& primitive = mesh.primitives[index];
            const std::string owner = Diagnostics::item("primitive", index, Diagnostics::item("mesh", meshIndex));
            const std::uint64_t vertices = document.accessors[primitive.attributes.front().accessor].count;
            if (const std::optional<std::size_t> normals = primitive.findAttribute("NORMAL")) {
                checks[*normals].normals = "the normals of " + owner;
            }

            if (const std::optional<std::size_t> tangents = primitive.findAttribute("TANGENT")) {
                checks[*tangents].tangents = "the tangents of " + owner;
            }

            const std::optional<std::size_t> colors = primitive.findAttribute("COLOR_0");
            if (colors && document.accessors[*colors].componentType == Accessor::ComponentType::Float) {
                checks[*colors].colors = "the colors of " + owner;
            }

            if (primitive.indices) {
                Checks& indices = checks[*primitive.indices];
                indices.vertices = indices.indices.empty() ? vertices : std::min(indices.vertices, vertices);
                indices.indices = "the indices of " + owner;
            }
        }
    }

    for (std::size_t index = 0; index < document.skins.size(); ++index) {
        const Skin& skin = document.skins[index];
        if (skin.inverseBindMatrices) {
            Checks& matrices = checks[*skin.inverseBindMatrices];
            matrices.inverseBindMatrices = "the inverse bind matrices of " + Diagnostics::item("skin", index);
            matrices.joints = std::max<std::uint64_t>(matrices.joints, skin.joints.size());
        }
    }

    for (std::size_t index = 0; index < document.animations.size(); ++index) {
        const Animation& animation = document.animations[index];
        const std::string owner = Diagnostics::item("animation", index);
        for (std::size_t sampler = 0; sampler < animation.samplers.size(); ++sampler) {
            checks[animation.samplers[sampler].input].times = "the times of " + Diagnostics::item("sampler", sampler, owner);
        }

        for (const Animation::Channel& channel : animation.channels) {
            if (channel.node && channel.path == Animation::Path::Rotation) {
                const Animation::Sampler& sampler = animation.samplers[channel.sampler];
                Checks& rotations = checks[sampler.output];
                rotations.rotations = "the rotations of " + Diagnostics::item("sampler", channel.sampler, owner);
                rotations.cubicSpline = sampler.interpolation == Animation::Interpolation::CubicSpline;
            }
        }
    }
}

void DataValidator::validateSparse(std::size_t accessor) const {
    const Accessor& found = document.accessors[accessor];
    if (!found.sparse) {
        return;
    }

    const AccessorView view(document, accessor);
    for (std::uint64_t position = 0; position < found.sparse->count; ++position) {
        const std::uint64_t index = view.getSparseIndex(position);
        if (index >= found.count) {
            diagnostics.invalid(Problem::InvalidSparseIndices, "the sparse index " + std::to_string(position) + " of accessor " + std::to_string(accessor) + " names element " + std::to_string(index) + ", past its " + std::to_string(found.count) + " elements");
        }

        if (position > 0 && index <= view.getSparseIndex(position - 1)) {
            diagnostics.invalid(Problem::InvalidSparseIndices, "the sparse indices of accessor " + std::to_string(accessor) + " do not grow at index " + std::to_string(position));
        }
    }
}

void DataValidator::fail(Problem problem, std::size_t accessor, std::uint64_t element, const std::string& use, std::string_view rule) const {
    const std::string subject = "element " + std::to_string(element) + " of accessor " + std::to_string(accessor) + (use.empty() ? "" : ", " + use + ",");
    diagnostics.invalid(problem, subject + " " + std::string(rule));
}

void DataValidator::validateAccessor(std::size_t accessor) const {
    const Accessor& found = document.accessors[accessor];
    const Checks& check = checks[accessor];
    const bool floats = found.componentType == Accessor::ComponentType::Float;
    const bool bounded = !found.min.empty() || !found.max.empty();
    const bool needed = floats || bounded || !check.indices.empty() || !check.times.empty() || !check.rotations.empty() || !check.inverseBindMatrices.empty();
    if (!needed) {
        return;
    }

    const std::size_t components = found.getComponentCount();
    std::array<double, AccessorView::kMaxComponents> lowest{};
    std::array<double, AccessorView::kMaxComponents> highest{};
    lowest.fill(std::numeric_limits<double>::infinity());
    highest.fill(-std::numeric_limits<double>::infinity());
    const double restart = FormatNames::getHighest(found.componentType);
    double previousTime = 0.0;

    // clang-format off
    const auto visit = [&](std::uint64_t element, const std::array<double, AccessorView::kMaxComponents>& values) {
        for (std::size_t component = 0; component < components; ++component) {
            const double value = values[component];
            if (floats && !std::isfinite(value)) {
                fail(Problem::NonFinite, accessor, element, std::string(), "is not a finite number");
            }

            lowest[component] = std::min(lowest[component], value);
            highest[component] = std::max(highest[component], value);
        }

        if (!check.normals.empty() || !check.tangents.empty()) {
            const double length = std::sqrt(values[0] * values[0] + values[1] * values[1] + values[2] * values[2]);
            if (std::abs(length - 1.0) > FormatNames::kUnitLengthThresholdVec3) {
                fail(Problem::NotUnit, accessor, element, check.normals.empty() ? check.tangents : check.normals, "is not a unit vector");
            }
        }

        if (!check.tangents.empty() && values[3] != 1.0 && values[3] != -1.0) {
            fail(Problem::InvalidSign, accessor, element, check.tangents, "gives a handedness other than 1 or -1");
        }

        if (!check.colors.empty()) {
            for (std::size_t component = 0; component < components; ++component) {
                if (values[component] < 0.0 || values[component] > 1.0) {
                    fail(Problem::NotClamped, accessor, element, check.colors, "holds a color outside the range from 0 to 1");
                }
            }
        }

        if (!check.indices.empty()) {
            if (values[0] == restart) {
                fail(Problem::PrimitiveRestart, accessor, element, check.indices, "is the largest value of its type, which restarts strips on some graphics APIs");
            }

            if (values[0] >= static_cast<double>(check.vertices)) {
                fail(Problem::IndexOutOfRange, accessor, element, check.indices, "names vertex " + std::to_string(static_cast<std::uint64_t>(values[0])) + " of the " + std::to_string(check.vertices) + " vertices of the primitive");
            }
        }

        if (!check.times.empty()) {
            if (values[0] < 0.0) {
                fail(Problem::InvalidTimes, accessor, element, check.times, "is a negative time");
            }

            if (element > 0 && values[0] <= previousTime) {
                fail(Problem::InvalidTimes, accessor, element, check.times, "is not later than the time before it");
            }

            previousTime = values[0];
        }

        // A cubic spline holds an in tangent, a value and an out tangent for each keyframe, and only the values are rotations. The length is that of the floats that the accessor reader gives, which the tracks of clips check again.
        if (!check.rotations.empty() && (!check.cubicSpline || element % 3 == 1)) {
            double squared = 0.0;
            for (std::size_t component = 0; component < 4; ++component) {
                const double value = static_cast<float>(AccessorView::normalize(found.componentType, values[component]));
                squared += value * value;
            }

            if (std::abs(std::sqrt(squared) - 1.0) > FormatNames::kUnitLengthThresholdVec4) {
                fail(Problem::NotUnit, accessor, element, check.rotations, "is not a unit quaternion");
            }
        }

        if (!check.inverseBindMatrices.empty() && element < check.joints && (values[3] != 0.0 || values[7] != 0.0 || values[11] != 0.0 || values[15] != 1.0)) {
            fail(Problem::InvalidInverseBindMatrix, accessor, element, check.inverseBindMatrices, "has a last row other than 0, 0, 0, 1");
        }
    };
    // clang-format on

    const AccessorView view(document, accessor);
    view.forEach(visit);

    // The bounds of floats match the data as floats, and the bounds of integers match the stored integers, also of normalized ones.
    for (std::size_t component = 0; component < components; ++component) {
        const bool minimumDiffers = !found.min.empty() && found.min[component] != lowest[component];
        const bool maximumDiffers = !found.max.empty() && found.max[component] != highest[component];
        if (minimumDiffers || maximumDiffers) {
            const std::string bound = minimumDiffers ? "\"min\"" : "\"max\"";
            const double declared = minimumDiffers ? found.min[component] : found.max[component];
            const double actual = minimumDiffers ? lowest[component] : highest[component];
            diagnostics.invalid(Problem::BoundsMismatch, "the bound " + bound + " of accessor " + std::to_string(accessor) + " gives " + core::Json(declared).dump() + " for component " + std::to_string(component) + ", while its data gives " + core::Json(actual).dump());
        }
    }
}

void DataValidator::validateSkinning() const {
    for (std::size_t meshIndex = 0; meshIndex < document.meshes.size(); ++meshIndex) {
        // The joints of a mesh name joints of every skin that bends it, so the skin with the fewest joints bounds them.
        std::optional<std::uint64_t> joints;
        for (const Node& node : document.nodes) {
            if (node.mesh == meshIndex && node.skin) {
                const std::uint64_t count = document.skins[*node.skin].joints.size();
                joints = std::min(joints.value_or(count), count);
            }
        }

        if (!joints) {
            continue;
        }

        const Mesh& mesh = document.meshes[meshIndex];
        for (std::size_t index = 0; index < mesh.primitives.size(); ++index) {
            validateInfluences(mesh.primitives[index], *joints, Diagnostics::item("primitive", index, Diagnostics::item("mesh", meshIndex)));
        }
    }
}

void DataValidator::validateInfluences(const Mesh::Primitive& primitive, std::uint64_t joints, const std::string& owner) const {
    std::vector<std::vector<std::uint32_t>> jointSets;
    std::vector<std::vector<float>> weightSets;
    for (std::uint32_t set = 0;; ++set) {
        const std::optional<std::size_t> jointAccessor = primitive.findAttribute("JOINTS_" + std::to_string(set));
        const std::optional<std::size_t> weightAccessor = primitive.findAttribute("WEIGHTS_" + std::to_string(set));
        if (!jointAccessor || !weightAccessor) {
            break;
        }

        jointSets.push_back(AccessorReader::readUnsigned(document, *jointAccessor));
        weightSets.push_back(AccessorReader::readFloats(document, *weightAccessor));
    }

    if (jointSets.empty()) {
        return;
    }

    const std::size_t vertices = jointSets.front().size() / 4;
    std::vector<std::uint32_t> seen;
    for (std::size_t vertex = 0; vertex < vertices; ++vertex) {
        float sum = 0.0F;
        double threshold = 0.0;
        seen.clear();
        for (std::size_t component = 0; component < 4; ++component) {
            for (std::size_t set = 0; set < jointSets.size(); ++set) {
                const std::uint32_t joint = jointSets[set][vertex * 4 + component];
                const float weight = weightSets[set][vertex * 4 + component];
                if (joint >= joints) {
                    diagnostics.invalid(Problem::InvalidJoint, "vertex " + std::to_string(vertex) + " of " + owner + " names joint " + std::to_string(joint) + " of a skin with " + std::to_string(joints) + " joints");
                }

                if (weight == 0.0F) {
                    continue;
                }

                if (weight < 0.0F) {
                    diagnostics.invalid(Problem::InvalidWeights, "vertex " + std::to_string(vertex) + " of " + owner + " has a negative weight");
                }

                if (std::ranges::find(seen, joint) != seen.end()) {
                    diagnostics.invalid(Problem::InvalidJoint, "vertex " + std::to_string(vertex) + " of " + owner + " names joint " + std::to_string(joint) + " twice with weights");
                }

                seen.push_back(joint);
                sum += weight;
                threshold += 2e-7;
            }
        }

        if (std::abs(static_cast<double>(sum) - 1.0) > threshold) {
            diagnostics.invalid(Problem::InvalidWeights, "the weights of vertex " + std::to_string(vertex) + " of " + owner + " add up to " + core::Json(sum).dump() + " and not to 1");
        }
    }
}

} // namespace haylen::gltf
