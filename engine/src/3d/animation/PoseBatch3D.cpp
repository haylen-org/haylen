#include "haylen/3d/animation/PoseBatch3D.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>
#include <utility>

#include "haylen/core/JobSystem.hpp"

namespace haylen::animation3d {

PoseBatch3D::PoseBatch3D(std::shared_ptr<const Skin3D> batchSkin) : skin(std::move(batchSkin)) {
    if (!skin) {
        throw std::invalid_argument("A pose batch needs the skin of its characters.");
    }
}

float PoseBatch3D::checkFinite(float value, const char* what) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::format("The {} of a character of a pose batch is a finite number, while it was given {}.", what, value));
    }

    return value;
}

float PoseBatch3D::wrap(float time, float duration) noexcept {
    if (duration <= 0.0F) {
        return 0.0F;
    }

    const float wrapped = std::fmod(time, duration);
    return wrapped < 0.0F ? wrapped + duration : wrapped;
}

// Characters of one clip share its sampler, and a sampler that no character plays any more leaves.
std::shared_ptr<const PoseSampler3D> PoseBatch3D::findSampler(const std::shared_ptr<const Clip3D>& clip) {
    if (!clip) {
        throw std::invalid_argument("A character of a pose batch needs a clip to play.");
    }

    const auto found = std::ranges::find(samplers, clip, [](const std::shared_ptr<const PoseSampler3D>& sampler) { return sampler->getClipPointer(); });
    if (found != samplers.end()) {
        return *found;
    }

    std::erase_if(samplers, [](const std::shared_ptr<const PoseSampler3D>& sampler) { return sampler.use_count() == 1; });
    samplers.push_back(std::make_shared<const PoseSampler3D>(clip, skin->getSkeletonPointer()));
    return samplers.back();
}

void PoseBatch3D::checkIndex(std::size_t index) const {
    if (index >= characters.size()) {
        throw std::out_of_range(std::format("The pose batch holds {} characters, so it has no character {}.", characters.size(), index));
    }
}

std::size_t PoseBatch3D::add(std::shared_ptr<const Clip3D> clip) {
    return add(std::move(clip), Options{});
}

std::size_t PoseBatch3D::add(std::shared_ptr<const Clip3D> clip, const Options& options) {
    const float time = checkFinite(options.time, "time");
    const float speed = checkFinite(options.speed, "speed");
    std::shared_ptr<const PoseSampler3D> sampler = findSampler(clip);
    const std::size_t cursorCount = sampler->getBoundTrackCount();
    characters.push_back({.sampler = std::move(sampler), .pose = Pose3D(skin->getSkeletonPointer()), .cursors = std::vector<std::size_t>(cursorCount, 0), .palette = std::vector<math::Mat4>(skin->getJointCount()), .time = wrap(time, clip->getDuration()), .speed = speed});
    palettes.resize(characters.size() * skin->getJointCount() * Skin3D::kRowFloats);
    pose(characters.size() - 1, 0.0F);
    return characters.size() - 1;
}

void PoseBatch3D::remove(std::size_t index) {
    checkIndex(index);
    characters.erase(characters.begin() + static_cast<std::ptrdiff_t>(index));
    const std::size_t rows = skin->getJointCount() * Skin3D::kRowFloats;
    palettes.erase(palettes.begin() + static_cast<std::ptrdiff_t>(index * rows), palettes.begin() + static_cast<std::ptrdiff_t>((index + 1) * rows));
}

void PoseBatch3D::setClip(std::size_t index, std::shared_ptr<const Clip3D> clip) {
    checkIndex(index);
    Character& character = characters[index];
    character.sampler = findSampler(clip);
    character.cursors.assign(character.sampler->getBoundTrackCount(), 0);
    character.time = wrap(character.time, clip->getDuration());
}

void PoseBatch3D::setTime(std::size_t index, float time) {
    checkIndex(index);
    Character& character = characters[index];
    character.time = wrap(checkFinite(time, "time"), character.sampler->getClip().getDuration());
}

void PoseBatch3D::setSpeed(std::size_t index, float speed) {
    checkIndex(index);
    characters[index].speed = checkFinite(speed, "speed");
}

float PoseBatch3D::getTime(std::size_t index) const {
    checkIndex(index);
    return characters[index].time;
}

const Pose3D& PoseBatch3D::getPose(std::size_t index) const {
    checkIndex(index);
    return characters[index].pose;
}

void PoseBatch3D::setTimes(std::span<const float> times) {
    if (times.size() != characters.size()) {
        throw std::invalid_argument(std::format("The pose batch holds {} characters, while {} times were given.", characters.size(), times.size()));
    }

    for (std::size_t index = 0; index < times.size(); ++index) {
        characters[index].time = wrap(checkFinite(times[index], "time"), characters[index].sampler->getClip().getDuration());
    }
}

void PoseBatch3D::pose(std::size_t index, float deltaSeconds) {
    Character& character = characters[index];
    const float duration = character.sampler->getClip().getDuration();
    character.time = wrap(character.time + deltaSeconds * character.speed, duration);
    character.pose.setRest();
    character.sampler->sample(character.time, character.pose.getLocalTransforms(), character.cursors);
    character.pose.computeModelMatrices();
    skin->computePalette(character.pose, character.palette);
    const std::size_t rows = skin->getJointCount() * Skin3D::kRowFloats;
    Skin3D::writeRows(character.palette, std::span(palettes).subspan(index * rows, rows));
}

void PoseBatch3D::update(float deltaSeconds, core::JobSystem* jobs) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F) {
        throw std::invalid_argument(std::format("A pose batch updates by a finite time step of zero or more seconds, while it was given {}.", deltaSeconds));
    }

    // clang-format off
    const auto run = [this, deltaSeconds](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            pose(index, deltaSeconds);
        }
    };
    // clang-format on
    if (jobs != nullptr && characters.size() >= kParallelCharacters) {
        jobs->parallelFor(0, characters.size(), kParallelGrain, run);
        return;
    }

    run(0, characters.size());
}

} // namespace haylen::animation3d
