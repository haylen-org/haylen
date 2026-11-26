#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "haylen/3d/animation/Clip3D.hpp"
#include "haylen/3d/animation/Pose3D.hpp"
#include "haylen/3d/animation/PoseSampler3D.hpp"
#include "haylen/3d/animation/Skin3D.hpp"
#include "haylen/math/Mat4.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::animation3d {

// Many characters of one skeleton and skin, such as a crowd, each looping a clip at its own time and speed. An update moves every time, poses every character and writes the palettes of all of them into one buffer of rows, the layout that one shared texture or buffer of joints takes for instanced drawing, spreading the characters over the job system.
class PoseBatch3D final {
  public:
    struct Options {
        float time = 0.0F;
        float speed = 1.0F;
    };

    // The characters from which an update spreads over the job system.
    static constexpr std::size_t kParallelCharacters = 8;

    // Throws `std::invalid_argument` for no skin.
    explicit PoseBatch3D(std::shared_ptr<const Skin3D> skin);

    [[nodiscard]] const Skin3D& getSkin() const noexcept {
        return *skin;
    }
    [[nodiscard]] std::size_t getCount() const noexcept {
        return characters.size();
    }

    // Adds a character that loops a clip and returns its index, counted from zero. Throws `std::invalid_argument` for no clip, a time or a speed that is not finite, and the errors of `PoseSampler3D`.
    std::size_t add(std::shared_ptr<const Clip3D> clip);
    std::size_t add(std::shared_ptr<const Clip3D> clip, const Options& options);

    // Removes a character, which moves the characters after it one index down. Throws `std::out_of_range` for an index the batch lacks.
    void remove(std::size_t index);

    // Each throws `std::out_of_range` for an index the batch lacks, and `std::invalid_argument` as `add` does.
    void setClip(std::size_t index, std::shared_ptr<const Clip3D> clip);
    void setTime(std::size_t index, float time);
    void setSpeed(std::size_t index, float speed);
    [[nodiscard]] float getTime(std::size_t index) const;

    // Sets the time of every character from the first, one number each. Throws `std::invalid_argument` for another count than the characters or a time that is not finite.
    void setTimes(std::span<const float> times);

    // Moves the time of every character by `deltaSeconds` times its speed, wrapped into its clip, and writes its palette. Allocates nothing without a job system. Throws `std::invalid_argument` for a time step that is not finite or below zero.
    void update(float deltaSeconds, core::JobSystem* jobs = nullptr);

    // The palettes of every character as of the last update, `Skin3D::kRowFloats` floats for each joint, the joints of each character one after another.
    [[nodiscard]] std::span<const float> getPalettes() const noexcept {
        return palettes;
    }

    // The pose of a character as of the last update. Throws `std::out_of_range` for an index the batch lacks.
    [[nodiscard]] const Pose3D& getPose(std::size_t index) const;

  private:
    struct Character {
        std::shared_ptr<const PoseSampler3D> sampler;
        Pose3D pose;
        std::vector<std::size_t> cursors;
        std::vector<math::Mat4> palette;
        float time = 0.0F;
        float speed = 1.0F;
    };

    static constexpr std::size_t kParallelGrain = 2;

    [[nodiscard]] std::shared_ptr<const PoseSampler3D> findSampler(const std::shared_ptr<const Clip3D>& clip);
    void checkIndex(std::size_t index) const;
    void pose(std::size_t index, float deltaSeconds);
    [[nodiscard]] static float wrap(float time, float duration) noexcept;
    [[nodiscard]] static float checkFinite(float value, const char* what);

    std::shared_ptr<const Skin3D> skin;
    std::vector<std::shared_ptr<const PoseSampler3D>> samplers;
    std::vector<Character> characters;
    std::vector<float> palettes;
};

} // namespace haylen::animation3d
