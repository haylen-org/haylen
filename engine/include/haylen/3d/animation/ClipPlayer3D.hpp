#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/3d/animation/Clip3D.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/core/Signal.hpp"

namespace haylen::animation3d {

// Plays one clip over time: it moves its time by the seconds of each update times its speed, which may be negative to play backward, wraps or turns at the ends as its loop mode says, samples every track of the clip into its values and reports the events it passes, in the order it passes them, on the thread that updates it.
class ClipPlayer3D final {
  public:
    // What happens at the ends of the clip: stop there and report the end, start again from the other end, or turn and play the other way.
    enum class Loop : std::uint8_t {
        Loop,
        Once,
        PingPong,
    };

    struct Options {
        Loop loop = Loop::Loop;
        float speed = 1.0F;
        bool paused = false;

        // The time to start at, which is the start of the clip, or its end while the speed is negative, without one.
        std::optional<float> time{};
    };

    // An update that passes the clip many times reports the events of the last passes it makes, at most this many.
    static constexpr int kMaxReportedPasses = 3;

    // Throws `std::invalid_argument` for no clip, a speed that is not finite and a time outside the clip.
    explicit ClipPlayer3D(std::shared_ptr<const Clip3D> clip);
    ClipPlayer3D(std::shared_ptr<const Clip3D> clip, const Options& options);

    [[nodiscard]] const Clip3D& getClip() const noexcept {
        return *clip;
    }
    [[nodiscard]] const std::shared_ptr<const Clip3D>& getClipPointer() const noexcept {
        return clip;
    }

    // Plays another clip from its start, or from its end while the speed is negative.
    void setClip(std::shared_ptr<const Clip3D> value);

    // The time in the clip, from zero to its duration.
    [[nodiscard]] float getTime() const noexcept;

    // Moves to a time without passing the events between, and reports the events at that time once the next update moves on from it. Throws `std::invalid_argument` for a time outside the clip.
    void setTime(float value);

    [[nodiscard]] float getSpeed() const noexcept {
        return speed;
    }

    // Throws `std::invalid_argument` for a speed that is not finite.
    void setSpeed(float value);
    [[nodiscard]] bool isPaused() const noexcept {
        return paused;
    }
    void setPaused(bool value) noexcept {
        paused = value;
    }
    [[nodiscard]] Loop getLoop() const noexcept {
        return loop;
    }
    void setLoop(Loop value) noexcept;

    // Whether a player of `Loop::Once` reached the end it plays toward.
    [[nodiscard]] bool isFinished() const noexcept {
        return finished;
    }

    // The values of every track as of the last update or time set, laid out as `Clip3D::sample` writes them.
    [[nodiscard]] std::span<const float> getValues() const noexcept {
        return values;
    }

    // Moves the time by `deltaSeconds` times the speed unless paused, samples the clip and then reports the events passed and the end of a clip that plays once. Allocates nothing once the events of one update fit what earlier updates held. Throws `std::invalid_argument` for a time step that is not finite or below zero, and `std::logic_error` when a listener of the player updates it.
    void update(float deltaSeconds);

    // Reports each event the player passes.
    core::Signal<const Clip3D::Event&> eventFired;

    // Reports that a player of `Loop::Once` reached the end it plays toward.
    core::Signal<> completed;

    [[nodiscard]] static std::optional<Loop> loopFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view loopName(Loop value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Loop>, 3> kLoopNames;

    // The length of one cycle of the phase: the duration, or twice it for a player that turns at the ends.
    [[nodiscard]] float getPeriod() const noexcept;
    [[nodiscard]] float timeOfPhase(float value) const noexcept;

    // Moves the phase and returns whether a player of `Loop::Once` reached its end with this move.
    [[nodiscard]] bool advance(float amount);
    void advanceForward(float amount);
    void advanceBackward(float amount);

    // Collects the events of the phases from `from` to `to`, forward when `to` lies after `from` and backward otherwise, with the events at `from` when `withStart` is set.
    void collect(float from, float to, bool withStart);
    void collectAt(float at, std::size_t event, float from, float to, bool forward, bool withStart);
    void resample();

    std::shared_ptr<const Clip3D> clip;
    Loop loop = Loop::Loop;
    float speed = 1.0F;
    bool paused = false;
    bool finished = false;

    // Whether the events at the current time wait to be reported, as after a start or a time set.
    bool fresh = true;
    bool emitting = false;

    // The place in one cycle: the time itself, or for a player that turns at the ends the time forward in the first half and back in the second.
    float phase = 0.0F;
    std::vector<float> values;
    std::vector<std::size_t> cursors;
    std::vector<std::size_t> passed;
};

} // namespace haylen::animation3d

namespace haylen::core {

template <> struct EnumNames<animation3d::ClipPlayer3D::Loop> {
    static std::optional<animation3d::ClipPlayer3D::Loop> fromName(std::string_view name) {
        return animation3d::ClipPlayer3D::loopFromName(name);
    }
    static std::string_view name(animation3d::ClipPlayer3D::Loop value) {
        return animation3d::ClipPlayer3D::loopName(value);
    }
};

} // namespace haylen::core
