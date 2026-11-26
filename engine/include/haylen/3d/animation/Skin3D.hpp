#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "haylen/3d/animation/Pose3D.hpp"
#include "haylen/3d/animation/Skeleton3D.hpp"
#include "haylen/math/DualQuat.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Quat.hpp"

namespace haylen::gltf {
struct Document;
}

namespace haylen::animation3d {

// The joints that bend a mesh: bones of a skeleton, each with the inverse bind matrix that brings a vertex of the mesh into the space of its bone at rest. The skin turns a pose into the palette of the joints, the matrices that the vertices of the mesh blend by their weights, with no limit on the number of joints other than the bones of a skeleton.
class Skin3D final {
  public:
    struct Joint {
        std::size_t bone = 0;
        math::Mat4 inverseBindMatrix{};
    };

    // The floats of one matrix of a palette in the rows that a texture or a buffer of joints holds: the first three rows of the matrix, four floats each, since the last row of an affine matrix is always 0, 0, 0 and 1.
    static constexpr std::size_t kRowFloats = 12;

    // Throws `std::invalid_argument` for no skeleton, no joints, a joint of a bone that the skeleton lacks and an inverse bind matrix that is not finite or not affine.
    Skin3D(std::shared_ptr<const Skeleton3D> skeleton, std::vector<Joint> joints);

    // Makes the skin of a glTF document for the skeleton that `Skeleton3D::fromGltf` made of it, with an identity for each joint when the skin has no inverse bind matrices. Throws `std::out_of_range` for a skin the document lacks and `std::invalid_argument` with a sentence that names the file for a skin that does not fit the skeleton.
    [[nodiscard]] static Skin3D fromGltf(const gltf::Document& document, std::size_t skin, std::shared_ptr<const Skeleton3D> skeleton);

    [[nodiscard]] const Skeleton3D& getSkeleton() const noexcept {
        return *skeleton;
    }
    [[nodiscard]] const std::shared_ptr<const Skeleton3D>& getSkeletonPointer() const noexcept {
        return skeleton;
    }
    [[nodiscard]] std::span<const Joint> getJoints() const noexcept {
        return joints;
    }
    [[nodiscard]] std::size_t getJointCount() const noexcept {
        return joints.size();
    }

    // Writes the matrix of each joint, the model matrix of its bone as of the last `Pose3D::computeModelMatrices` times its inverse bind matrix. Allocates nothing. Throws `std::invalid_argument` for a pose of another skeleton or a palette of another size than the joints.
    void computePalette(const Pose3D& pose, std::span<math::Mat4> palette) const;

    // Writes the rotation and translation of each joint as a dual quaternion, which blends without the loss of volume of blended matrices at twisted joints. Dual quaternions hold no scale, so the rotation of each joint comes from the rotations of the pose and of its inverse bind matrix, and a skin whose joints scale blends its matrices instead. Allocates nothing. Throws as `computePalette` does, and `std::domain_error` for a joint whose inverse bind matrix scales an axis to zero, which holds no rotation.
    void computeDualQuatPalette(const Pose3D& pose, std::span<math::DualQuat> palette) const;

    // Writes each matrix of a palette as `kRowFloats` floats into `rows`. Throws `std::invalid_argument` for rows of another size.
    static void writeRows(std::span<const math::Mat4> palette, std::span<float> rows);

  private:
    void checkPose(const Pose3D& pose, std::size_t paletteSize) const;

    std::shared_ptr<const Skeleton3D> skeleton;
    std::vector<Joint> joints;
    std::vector<math::Mat4> inverseBindMatrices;

    // The rotation of each inverse bind matrix, which the dual quaternions of the joints take, or nothing for a matrix that scales an axis to zero.
    std::vector<std::optional<math::Quat>> bindRotations;
};

} // namespace haylen::animation3d
