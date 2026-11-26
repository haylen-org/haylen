#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/animation/Track3D.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::gltf {
struct Document;
}

namespace haylen::animation3d {

// An animation of a model or a scene: tracks that move the properties of its nodes, bones and components over the same time, and named events at moments of it, such as a footstep. A clip is data that many players and characters share, and it never changes once made, so every edit returns a new clip.
class Clip3D final {
  public:
    // A moment of the clip that players report as they pass it, with the data that the app gave it.
    struct Event {
        float time = 0.0F;
        std::string name{};
        core::Json data{};
    };

    // The parts of a new clip. Without a duration, the clip lasts until its last key or event.
    struct Description {
        std::string name{};
        std::vector<Track3D> tracks{};
        std::vector<Event> events{};
        std::optional<float> duration{};
    };

    // A clip that plays the morph targets of a mesh one after another, each one fully at its frame and fading into the next.
    struct MorphSequence {
        std::string name{};
        std::string target{};
        std::size_t targetCount = 0;
        float framesPerSecond = 30.0F;

        // Fades the last target back into the first, so a looping player wraps without a jump.
        bool loop = true;
    };

    // Throws `std::invalid_argument` for two tracks of the same property of one target, an event at a time that is not finite or below zero or without a name, and a duration shorter than a track or an event.
    explicit Clip3D(Description description);

    // Makes the clip of an animation of a glTF document. Its tracks name each node by its name when no other node of the file takes it, or by the glTF pointer of the node, such as `/nodes/3`, otherwise, so a clip of another file whose nodes carry the same names moves the same nodes. Throws `std::out_of_range` for an animation the document lacks and `std::invalid_argument` with a sentence that names the file and the animation for data that does not make a clip.
    [[nodiscard]] static Clip3D fromGltf(const gltf::Document& document, std::size_t animation);

    // Throws `std::invalid_argument` for no targets, a rate that is not positive or more keys than `Track3D::kMaxKeys`.
    [[nodiscard]] static Clip3D fromMorphSequence(const MorphSequence& sequence);

    [[nodiscard]] const std::string& getName() const noexcept {
        return name;
    }
    [[nodiscard]] float getDuration() const noexcept {
        return duration;
    }
    [[nodiscard]] std::span<const Track3D> getTracks() const noexcept {
        return tracks;
    }

    // The events in the order of their times.
    [[nodiscard]] std::span<const Event> getEvents() const noexcept {
        return events;
    }

    // The numbers that a sample writes: the components of every track, one after another in the order of the tracks.
    [[nodiscard]] std::size_t getValueCount() const noexcept {
        return valueCount;
    }

    // The place of the first number of a track in what a sample writes. Throws `std::out_of_range` for a track the clip lacks.
    [[nodiscard]] std::size_t getValueOffset(std::size_t track) const;
    [[nodiscard]] std::optional<std::size_t> findTrack(std::string_view target, std::string_view property) const noexcept;

    // The bytes that the keys of every track take.
    [[nodiscard]] std::size_t getMemorySize() const noexcept;

    // Writes the value of every track at a time into `values`, which holds at least `getValueCount()` numbers, with one cursor for each track that keeps its key from one sample to the next. A time before zero or after the end gives the values at the ends. Allocates nothing. Throws `std::invalid_argument` for buffers that are too small, and what `Track3D::sample` throws.
    void sample(float time, std::span<float> values, std::span<std::size_t> cursors) const;

    [[nodiscard]] Clip3D renamed(std::string value) const;

    // Returns the part from `start` to `end`, moved to start at zero, with its events. Throws `std::invalid_argument` for ends that are not finite, not in order or past the clip.
    [[nodiscard]] Clip3D trimmed(float start, float end) const;

    // Returns this clip followed by `next`, whose tracks join the tracks of the same property and target and whose events follow. A track of one clip alone holds its value through the other. Throws `std::invalid_argument` for tracks of one property that differ in type, interpolation or count of components.
    [[nodiscard]] Clip3D joined(const Clip3D& next) const;

    // Returns the clip with keys at a fixed rate for every track that interpolates, while tracks of steps keep their keys. Throws what `Track3D::resampled` throws.
    [[nodiscard]] Clip3D resampled(float framesPerSecond) const;

    // Returns the clip with the fewest keys that bring back every value within `tolerance`, as `Track3D::optimized` describes.
    [[nodiscard]] Clip3D optimized(float tolerance) const;

    // Returns the clip with fewer keys and with its values in 16-bit steps where that keeps them within `tolerance`, in the units of each track and in radians for rotations. Tracks whose range is too large for steps that fine, and tracks of booleans, strings and Bezier curves, keep their floats.
    [[nodiscard]] Clip3D compressed(float tolerance) const;

    // Returns the clip relative to the pose of `reference` at `time`, which an additive layer adds over another pose: positions and other values less the reference, scales divided by it and rotations turned back by it. Tracks of booleans and strings stay as they are. Throws `std::invalid_argument` for a track that the reference lacks.
    [[nodiscard]] Clip3D toAdditive(const Clip3D& reference, float time) const;

  private:
    std::string name;
    std::vector<Track3D> tracks;
    std::vector<Event> events;
    std::vector<std::size_t> offsets;
    std::size_t valueCount = 0;
    float duration = 0.0F;
};

} // namespace haylen::animation3d
