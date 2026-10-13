#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/animation/SkeletonData.hpp"
#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/MeshVertex.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::animation2d {

// One posed instance of a skeleton. Tracks play animations over each other in track order, each with a weight and a crossfade from the animation it played before, so a walk on one track and an aim on another mix. Every update advances the tracks, poses the bones from the setup pose, solves the constraints and computes the world transforms, and a draw sends every slot in draw order, regions and meshes alike, as meshes of the atlas texture.
class Skeleton final {
  public:
    struct BonePose {
        math::Vec2 position{};
        float rotation = 0.0F;
        math::Vec2 scale{1.0F, 1.0F};
    };

    struct SlotPose {
        const SkeletonData::Attachment* attachment = nullptr;
        math::Color color = math::Color::white();

        // The offsets a deform timeline gives the vertices of a mesh, two floats each, empty without a deform.
        std::vector<float> deform;
    };

    // An event of an animation that a track passed, with the track and the animation it came from.
    struct Event {
        std::size_t track = 0;
        std::string_view animation;
        std::string_view name;
        std::string_view text;
        float number = 0.0F;
    };

    struct PlayOptions {
        std::size_t track = 0;
        bool loop = true;
        float fade = 0.0F;
    };

    explicit Skeleton(std::shared_ptr<const SkeletonData> skeletonData);

    [[nodiscard]] const SkeletonData& getData() const noexcept {
        return *data;
    }
    [[nodiscard]] const std::shared_ptr<const SkeletonData>& getDataPointer() const noexcept {
        return data;
    }

    // Each throws `std::invalid_argument` for a name the data lacks.
    void setSkin(std::string_view name);
    void setAttachment(std::string_view slot, std::string_view name);
    [[nodiscard]] const std::string& getSkin() const noexcept;

    // Plays an animation on a track from its start, crossfading from the animation the track played over the fade in seconds.
    void play(std::string_view animation, const PlayOptions& options);
    void stop(std::size_t track);
    void setTrackWeight(std::size_t track, float value);
    [[nodiscard]] float getTrackWeight(std::size_t track) const;
    [[nodiscard]] std::string_view getTrackAnimation(std::size_t track) const;
    [[nodiscard]] float getTrackTime(std::size_t track) const;

    // Moves the animation of a track to a time, without the events it skips.
    void seekTrack(std::size_t track, float time);

    // Makes an inverse kinematics constraint reach a point in the world instead of its target bone, until cleared, and sets the mix it starts every pose with, which its timelines change.
    void setIkTarget(std::string_view constraint, math::Vec2 point);
    void clearIkTarget(std::string_view constraint);
    void setIkMix(std::string_view constraint, float value);

    // Advances the tracks, poses the skeleton and emits the events the tracks passed, in track order.
    void update(float deltaSeconds);

    // Poses the bones from the setup pose and the tracks at their current times, without advancing them.
    void pose();

    // The world transform of a bone and its point, rotation and scale, as of the last pose.
    [[nodiscard]] const math::Transform2D& getBoneWorld(std::size_t bone) const noexcept {
        return worlds[bone];
    }
    [[nodiscard]] std::optional<std::size_t> findBone(std::string_view name) const noexcept {
        return data->findBone(name);
    }
    [[nodiscard]] BonePose& getBonePose(std::size_t bone) noexcept {
        return bones[bone];
    }

    // Returns the point attachment of a slot in the world, or nothing when the slot shows another kind.
    [[nodiscard]] std::optional<math::Vec2> getPoint(std::string_view slot) const;

    // Returns the rectangle around every region and mesh of the last pose in the world.
    [[nodiscard]] math::Rect getBounds();

    void draw(graphics2d::Renderer& renderer, const graphics2d::DrawOrder& order = {});

    // The place of the root of the skeleton in the world, and a color that multiplies every slot.
    math::Vec2 position{};
    float rotation = 0.0F;
    math::Vec2 scale{1.0F, 1.0F};
    math::Color color = math::Color::white();

    core::Signal<const Event&> eventFired;

  private:
    friend class SkeletonAnimation;

    struct Entry {
        const SkeletonAnimation* animation = nullptr;
        float time = 0.0F;
        float previousTime = 0.0F;
        bool loop = true;
    };

    // The posed state that animations write, which a crossfade keeps a copy of to blend between two poses.
    struct Snapshot {
        std::vector<BonePose> bones;
        std::vector<SlotPose> slots;
        std::vector<std::size_t> drawOrder;
        std::vector<float> ikMixes;
    };

    struct Track {
        std::optional<Entry> current;
        std::optional<Entry> previous;
        float fade = 0.0F;
        float fadeDuration = 0.0F;
        float weight = 1.0F;
    };

    [[nodiscard]] Track& getTrack(std::size_t index);
    [[nodiscard]] std::size_t findIkConstraint(std::string_view name) const;
    void setToSetupPose();
    void save(Snapshot& snapshot) const;
    void restore(const Snapshot& snapshot);

    // Poses a track that crossfades: both animations apply over the pose before the track, and the pose becomes their blend by the share of the fade, mixed in by the weight of the track.
    void crossfade(const Track& track, float mix);
    void computeWorld();
    void solveIk(std::size_t constraint);
    void solveTransform(const SkeletonData::TransformConstraint& constraint);

    // Turns a bone so its world angle becomes `worldAngle`, mixed by the share.
    void aimBone(std::size_t bone, float worldAngle, float mix);
    [[nodiscard]] float worldAngleOf(std::size_t bone) const noexcept;

    // Appends the world vertices of the attachment of a slot to the vertices of the frame.
    void appendSlot(std::size_t slot);

    std::shared_ptr<const SkeletonData> data;
    std::size_t skin = 0;
    std::vector<BonePose> bones;
    std::vector<math::Transform2D> worlds;
    std::vector<SlotPose> slots;
    std::vector<std::size_t> drawOrder;
    std::vector<float> ikMixes;
    std::vector<float> ikSetupMixes;
    std::vector<bool> ikBends;
    std::vector<std::optional<math::Vec2>> ikTargets;
    std::vector<Track> tracks;

    // Reused by every update and draw, so a skeleton allocates nothing once they have grown.
    std::vector<const SkeletonAnimation::Event*> passed;
    Snapshot before;
    Snapshot faded;
    std::vector<graphics2d::MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

} // namespace haylen::animation2d
