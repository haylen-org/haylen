#include "haylen/3d/animation/PoseSampler3D.hpp"

#include <array>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

namespace haylen::animation3d {

PoseSampler3D::PoseSampler3D(std::shared_ptr<const Clip3D> sampled, std::shared_ptr<const Skeleton3D> posed) : clip(std::move(sampled)), skeleton(std::move(posed)) {
    if (!clip || !skeleton) {
        throw std::invalid_argument("A pose sampler needs a clip and the skeleton it poses.");
    }

    const std::span<const Track3D> tracks = clip->getTracks();
    for (std::size_t index = 0; index < tracks.size(); ++index) {
        const Track3D& track = tracks[index];
        const std::optional<std::size_t> bone = skeleton->findBone(track.getTarget());
        if (!bone) {
            continue;
        }

        const std::string& property = track.getProperty();
        Part part = Part::Position;
        Track3D::Type expected = Track3D::Type::Vec3;
        if (property == "rotation") {
            part = Part::Rotation;
            expected = Track3D::Type::Quat;
        } else if (property == "scale") {
            part = Part::Scale;
        } else if (property != "position") {
            continue;
        }

        if (track.getType() != expected) {
            std::string message = std::format("{} of the clip \"{}\" holds {} values, while the {} of a bone takes {} values.", track.describe(), clip->getName(), Track3D::typeName(track.getType()), property, Track3D::typeName(expected));
            message.front() = 'T';
            throw std::invalid_argument(message);
        }

        bindings.push_back({.track = index, .bone = *bone, .part = part});
    }
}

void PoseSampler3D::sample(float time, std::span<math::Transform3D> local, std::span<std::size_t> cursors) const {
    if (local.size() != skeleton->getBoneCount() || cursors.size() != bindings.size()) {
        throw std::invalid_argument(std::format("A sample of the clip \"{}\" into a pose takes {} bones and {} cursors, while {} and {} were given.", clip->getName(), skeleton->getBoneCount(), bindings.size(), local.size(), cursors.size()));
    }

    const std::span<const Track3D> tracks = clip->getTracks();
    std::array<float, 4> value{};
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        const Binding& binding = bindings[index];
        math::Transform3D& transform = local[binding.bone];
        switch (binding.part) {
        case Part::Position:
            tracks[binding.track].sample(time, std::span(value).first(3), cursors[index]);
            transform.position = {value[0], value[1], value[2]};
            break;
        case Part::Rotation:
            tracks[binding.track].sample(time, value, cursors[index]);
            transform.rotation = {value[0], value[1], value[2], value[3]};
            break;
        case Part::Scale:
            tracks[binding.track].sample(time, std::span(value).first(3), cursors[index]);
            transform.scale = {value[0], value[1], value[2]};
            break;
        }
    }
}

} // namespace haylen::animation3d
