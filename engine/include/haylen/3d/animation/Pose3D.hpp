#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "haylen/3d/animation/Skeleton3D.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Transform3D.hpp"

namespace haylen::animation3d {

// The pose of one character of a skeleton: the local transform of each bone relative to its parent, which clips write, and the matrix of each bone in the space of the skeleton, which skins read.
class Pose3D final {
  public:
    // Starts at the rest pose of the skeleton. Throws `std::invalid_argument` for no skeleton.
    explicit Pose3D(std::shared_ptr<const Skeleton3D> skeleton);

    [[nodiscard]] const Skeleton3D& getSkeleton() const noexcept {
        return *skeleton;
    }
    [[nodiscard]] const std::shared_ptr<const Skeleton3D>& getSkeletonPointer() const noexcept {
        return skeleton;
    }

    [[nodiscard]] std::span<math::Transform3D> getLocalTransforms() noexcept {
        return local;
    }
    [[nodiscard]] std::span<const math::Transform3D> getLocalTransforms() const noexcept {
        return local;
    }

    // The matrices as of the last `computeModelMatrices`.
    [[nodiscard]] std::span<const math::Mat4> getModelMatrices() const noexcept {
        return model;
    }

    // Puts every bone back at its rest pose.
    void setRest() noexcept;

    // Computes the matrix of every bone in the space of the skeleton from the local transforms. Allocates nothing.
    void computeModelMatrices();

  private:
    std::shared_ptr<const Skeleton3D> skeleton;
    std::vector<math::Transform3D> local;
    std::vector<math::Mat4> model;
};

} // namespace haylen::animation3d
