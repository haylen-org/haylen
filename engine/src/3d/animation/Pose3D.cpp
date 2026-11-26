#include "haylen/3d/animation/Pose3D.hpp"

#include <stdexcept>
#include <utility>

namespace haylen::animation3d {

Pose3D::Pose3D(std::shared_ptr<const Skeleton3D> posed) : skeleton(std::move(posed)) {
    if (!skeleton) {
        throw std::invalid_argument("A pose needs the skeleton it poses.");
    }

    local.resize(skeleton->getBoneCount());
    model.resize(skeleton->getBoneCount());
    setRest();
    computeModelMatrices();
}

void Pose3D::setRest() noexcept {
    const std::span<const Skeleton3D::Bone> bones = skeleton->getBones();
    for (std::size_t bone = 0; bone < bones.size(); ++bone) {
        local[bone] = bones[bone].rest;
    }
}

void Pose3D::computeModelMatrices() {
    skeleton->computeModelMatrices(local, model);
}

} // namespace haylen::animation3d
