#pragma once

#include <cstdint>

namespace haylen::ui {

// The offset of one scrolling axis and the motion a player expects from it: a finger drags the content and stretches it past the ends with a resistance that grows the farther it goes, a release flings it with a speed that decays, content beyond an end springs back, the steps of a wheel glide to their place, the points of a trackpad move the content at once, and a scroll to a place follows its target with a critically damped spring, also while the target moves. Offsets are doubles, so content millions of units long keeps its precision.
class Scroller final {
  public:
    // Sets how far the content scrolls and how long the view is, which bounds how far the content stretches past an end. The offset stays within the new range, at once while nothing moves and through the spring of a motion otherwise.
    void setRange(double maximumValue, double viewLength) noexcept;

    [[nodiscard]] double getOffset() const noexcept {
        return offset;
    }
    [[nodiscard]] double getMaximum() const noexcept {
        return maximum;
    }

    // How far the offset lies past an end, negative before the start.
    [[nodiscard]] double getOverscroll() const noexcept;
    [[nodiscard]] double getVelocity() const noexcept {
        return velocity;
    }
    [[nodiscard]] double getTarget() const noexcept {
        return target;
    }
    [[nodiscard]] bool isAnimating() const noexcept {
        return motion == Motion::Animating;
    }
    [[nodiscard]] bool isResting() const noexcept {
        return motion == Motion::Resting;
    }

    // Jumps to an offset within the range and stops.
    void jumpTo(double value) noexcept;

    // Moves the offset and the target of an animation together without changing the motion, as anchoring does when the content before the view changes.
    void shift(double delta) noexcept;

    // Moves the content with a finger by a distance over a time, by less and less of it past an end.
    void drag(double delta, float seconds) noexcept;

    // Moves the content by the wheel: steps of a wheel glide there, and the points of a trackpad move it at once, stretching past an end, until the trackpad rests and the content springs back.
    void wheel(double delta, bool stepped) noexcept;

    // Lets the finger go: the content flings with the speed it had, or springs back when it lies past an end.
    void release() noexcept;

    // Starts or retargets a smooth scroll to an offset within the range.
    void animateTo(double value) noexcept;
    void stop() noexcept;

    // Advances the motion and returns whether the offset moved.
    bool update(float seconds) noexcept;

  private:
    enum class Motion : std::uint8_t {
        Resting,
        Dragging,
        Wheeling,
        Flinging,
        Returning,
        Animating,
    };

    // A fling loses this share of its speed per second as the exponent of a decay, and stops below the stop speed.
    static constexpr double kFriction = 2.4;
    static constexpr double kMaxSpeed = 12000.0;
    static constexpr double kStopSpeed = 12.0;

    // The angular frequency of the springs that return from past an end and follow a target.
    static constexpr double kSpring = 18.0;
    static constexpr double kSettleDistance = 0.5;

    // Content pulled past an end by a distance stretches by the share of the view this constant gives, which nears the whole view only for a pull far longer than it.
    static constexpr double kElasticity = 0.55;

    // A trackpad that sends nothing for this long has come to rest.
    static constexpr float kWheelRest = 0.06F;

    [[nodiscard]] double clamp(double value) const noexcept;

    // Turns how far a pull reaches past an end into how far the content stretches, and back.
    [[nodiscard]] double stretch(double pull) const noexcept;
    [[nodiscard]] double unstretch(double overscroll) const noexcept;

    // Moves the offset by a distance of a pull, stretching past an end.
    void pull(double delta) noexcept;

    // Moves toward a goal the way a critically damped spring does, exactly for any step, and rests once it arrives.
    void spring(double goal, float seconds) noexcept;

    double offset = 0.0;
    double maximum = 0.0;
    double view = 0.0;
    double velocity = 0.0;
    double target = 0.0;
    float quiet = 0.0F;
    Motion motion = Motion::Resting;
};

} // namespace haylen::ui
