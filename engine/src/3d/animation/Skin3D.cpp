#include "haylen/3d/animation/Skin3D.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

#include "3d/animation/GltfNodeNames3D.hpp"
#include "haylen/3d/gltf/AccessorReader.hpp"
#include "haylen/3d/gltf/Document.hpp"
#include "haylen/math/MatrixBatch.hpp"

namespace haylen::animation3d {

Skin3D::Skin3D(std::shared_ptr<const Skeleton3D> bent, std::vector<Joint> skinJoints) : skeleton(std::move(bent)), joints(std::move(skinJoints)) {
    if (!skeleton) {
        throw std::invalid_argument("A skin needs the skeleton whose bones bend it.");
    }

    if (joints.empty() || joints.size() > Skeleton3D::kMaxBones) {
        throw std::invalid_argument(std::format("A skin holds from 1 to {} joints, while it was given {}.", Skeleton3D::kMaxBones, joints.size()));
    }

    inverseBindMatrices.reserve(joints.size());
    bindRotations.reserve(joints.size());
    for (std::size_t index = 0; index < joints.size(); ++index) {
        const Joint& joint = joints[index];
        if (joint.bone >= skeleton->getBoneCount()) {
            throw std::invalid_argument(std::format("Joint {} of a skin names bone {} of a skeleton of {} bones.", index, joint.bone, skeleton->getBoneCount()));
        }

        const bool finite = std::ranges::all_of(joint.inverseBindMatrix.values, [](float value) { return std::isfinite(value); });
        if (!finite || !joint.inverseBindMatrix.isAffine()) {
            throw std::invalid_argument(std::format("The inverse bind matrix of joint {} of a skin is not a finite affine matrix, whose last row is 0, 0, 0 and 1.", index));
        }

        const math::Mat3 linear = joint.inverseBindMatrix.getLinear();
        const bool flat = linear.getColumn(0).isZero() || linear.getColumn(1).isZero() || linear.getColumn(2).isZero();
        inverseBindMatrices.push_back(joint.inverseBindMatrix);
        bindRotations.push_back(flat ? std::nullopt : std::optional(joint.inverseBindMatrix.decompose().rotation));
    }
}

Skin3D Skin3D::fromGltf(const gltf::Document& document, std::size_t skin, std::shared_ptr<const Skeleton3D> skeleton) {
    if (skin >= document.skins.size()) {
        throw std::out_of_range(std::format("The glTF file \"{}\" has {} skins, so it has no skin {}.", document.file, document.skins.size(), skin));
    }

    if (!skeleton) {
        throw std::invalid_argument("A skin needs the skeleton whose bones bend it.");
    }

    const gltf::Skin& source = document.skins[skin];
    std::vector<float> matrices;
    if (source.inverseBindMatrices) {
        if (*source.inverseBindMatrices >= document.accessors.size() || document.accessors[*source.inverseBindMatrices].type != gltf::Accessor::Type::Mat4) {
            throw std::invalid_argument(std::format("The inverse bind matrices of skin {} of the glTF file \"{}\" are not an accessor of matrices of the file. Export the model again from the tool that made it, so it follows glTF 2.0.", skin, document.file));
        }

        matrices = gltf::AccessorReader::readFloats(document, *source.inverseBindMatrices);
        if (matrices.size() < source.joints.size() * 16) {
            throw std::invalid_argument(std::format("Skin {} of the glTF file \"{}\" holds fewer inverse bind matrices than its {} joints. Export the model again from the tool that made it, so it follows glTF 2.0.", skin, document.file, source.joints.size()));
        }
    }

    const std::vector<std::string> names = GltfNodeNames3D::of(document);
    std::vector<Joint> skinJoints;
    skinJoints.reserve(source.joints.size());
    for (std::size_t index = 0; index < source.joints.size(); ++index) {
        const std::size_t node = source.joints[index];
        const std::optional<std::size_t> bone = node < names.size() ? skeleton->findBone(names[node]) : std::nullopt;
        if (!bone) {
            throw std::invalid_argument(std::format("Joint {} of skin {} of the glTF file \"{}\" is node {}, which the skeleton has no bone of. Make the skeleton of the same skin.", index, skin, document.file, node));
        }

        Joint joint{.bone = *bone};
        if (!matrices.empty()) {
            std::copy_n(matrices.begin() + static_cast<std::ptrdiff_t>(index * 16), 16, joint.inverseBindMatrix.values.begin());
        }

        skinJoints.push_back(joint);
    }

    try {
        return Skin3D(std::move(skeleton), std::move(skinJoints));
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::format("The skin {} of the glTF file \"{}\" does not make a skin. {}", skin, document.file, error.what()));
    }
}

void Skin3D::checkPose(const Pose3D& pose, std::size_t paletteSize) const {
    if (pose.getSkeletonPointer() != skeleton) {
        throw std::invalid_argument("The pose belongs to another skeleton than the skin.");
    }

    if (paletteSize != joints.size()) {
        throw std::invalid_argument(std::format("The palette of a skin of {} joints holds {} entries, while {} were given.", joints.size(), joints.size(), paletteSize));
    }
}

void Skin3D::computePalette(const Pose3D& pose, std::span<math::Mat4> palette) const {
    checkPose(pose, palette.size());
    const std::span<const math::Mat4> model = pose.getModelMatrices();
    for (std::size_t index = 0; index < joints.size(); ++index) {
        palette[index] = model[joints[index].bone];
    }

    math::MatrixBatch::multiplyPairs(palette, inverseBindMatrices, palette);
}

// The rotation of a bone in the space of the skeleton is the rotation of each bone above it in turn, which holds even where a scale of zero hides a bone from its matrix.
void Skin3D::computeDualQuatPalette(const Pose3D& pose, std::span<math::DualQuat> palette) const {
    checkPose(pose, palette.size());
    const std::span<const math::Mat4> model = pose.getModelMatrices();
    const std::span<const math::Transform3D> local = pose.getLocalTransforms();
    const std::span<const Skeleton3D::Bone> bones = skeleton->getBones();
    for (std::size_t index = 0; index < joints.size(); ++index) {
        const std::size_t bone = joints[index].bone;
        math::Quat rotation = local[bone].rotation;
        for (std::optional<std::size_t> parent = bones[bone].parent; parent; parent = bones[*parent].parent) {
            rotation = local[*parent].rotation * rotation;
        }

        if (!bindRotations[index]) {
            throw std::domain_error(std::format("The inverse bind matrix of joint {} of the skin scales an axis to zero, so it holds no rotation for dual quaternion skinning. Skin it with matrices.", index));
        }

        const math::Vec3 translation = model[bone].apply(inverseBindMatrices[index].getTranslation());
        palette[index] = math::DualQuat::fromRotationTranslation((rotation * *bindRotations[index]).getNormalized(), translation);
    }
}

void Skin3D::writeRows(std::span<const math::Mat4> palette, std::span<float> rows) {
    if (rows.size() != palette.size() * kRowFloats) {
        throw std::invalid_argument(std::format("The rows of a palette of {} joints take {} floats, while {} were given.", palette.size(), palette.size() * kRowFloats, rows.size()));
    }

    for (std::size_t index = 0; index < palette.size(); ++index) {
        float* row = rows.data() + index * kRowFloats;
        for (std::size_t line = 0; line < 3; ++line) {
            for (std::size_t column = 0; column < 4; ++column) {
                row[line * 4 + column] = palette[index].get(line, column);
            }
        }
    }
}

} // namespace haylen::animation3d
