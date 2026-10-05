#pragma once

#include <cstdint>

namespace haylen::ui {

// The offset of one scrolling axis and the motion a player expects from it: a finger drags the content and stretches it past the ends with resistance, a release flings it with a speed that decays, content beyond an end springs back, and a scroll to a place follows its target with a critically damped spring, also while the target moves. Offsets are doubles, so content millions of units long keeps its precision.
class Scroller final {
  public:
    // Keeps the offset within the new range, at once while nothing moves and through the spring of a motion otherwise.
    void setMaximum(double value) noexcept;

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
    [[nodiscard]] bool isDragging() const noexcept {
        return motion == Motion::Dragging;
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

    // Moves the content with a finger by a distance over a time, at half the distance past an end.
    void drag(double delta, float seconds) noexcept;

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
    static constexpr double kResistance = 0.5;
    static constexpr double kSettleDistance = 0.5;

    [[nodiscard]] double clamp(double value) const noexcept;

    // Moves toward a goal the way a critically damped spring does, exactly for any step, and rests once it arrives.
    void spring(double goal, float seconds) noexcept;

    double offset = 0.0;
    double maximum = 0.0;
    double velocity = 0.0;
    double target = 0.0;
    Motion motion = Motion::Resting;
};

} // namespace haylen::ui
