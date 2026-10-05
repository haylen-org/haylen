#include "ui/Scroller.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::ui {

double Scroller::clamp(double value) const noexcept {
    return std::clamp(value, 0.0, maximum);
}

void Scroller::setMaximum(double value) noexcept {
    maximum = std::max(0.0, value);
    target = clamp(target);
    if (motion == Motion::Resting) {
        offset = clamp(offset);
    } else if (motion == Motion::Flinging && offset != clamp(offset)) {
        motion = Motion::Returning;
    }
}

double Scroller::getOverscroll() const noexcept {
    return offset - clamp(offset);
}

void Scroller::jumpTo(double value) noexcept {
    offset = clamp(value);
    target = offset;
    velocity = 0.0;
    motion = Motion::Resting;
}

void Scroller::shift(double delta) noexcept {
    offset += delta;
    target = clamp(target + delta);
    if (motion == Motion::Resting) {
        offset = clamp(offset);
    }
}

void Scroller::drag(double delta, float seconds) noexcept {
    if (motion != Motion::Dragging) {
        motion = Motion::Dragging;
        velocity = 0.0;
    }
    const bool stretching = (offset <= 0.0 && delta < 0.0) || (offset >= maximum && delta > 0.0);
    offset += stretching ? delta * kResistance : delta;
    if (seconds > 0.0F) {
        velocity = velocity * 0.4 + delta / static_cast<double>(seconds) * 0.6;
    }
}

void Scroller::release() noexcept {
    if (getOverscroll() != 0.0) {
        motion = Motion::Returning;
    } else if (std::fabs(velocity) > kStopSpeed) {
        velocity = std::clamp(velocity, -kMaxSpeed, kMaxSpeed);
        motion = Motion::Flinging;
    } else {
        stop();
    }
}

void Scroller::animateTo(double value) noexcept {
    target = clamp(value);
    if (motion != Motion::Animating) {
        velocity = motion == Motion::Dragging ? 0.0 : velocity;
        motion = Motion::Animating;
    }
}

void Scroller::stop() noexcept {
    velocity = 0.0;
    target = clamp(offset);
    motion = Motion::Resting;
}

void Scroller::spring(double goal, float seconds) noexcept {
    const double time = seconds;
    const double distance = offset - goal;
    const double rate = velocity + kSpring * distance;
    const double decay = std::exp(-kSpring * time);
    offset = goal + (distance + rate * time) * decay;
    velocity = (rate - kSpring * (distance + rate * time)) * decay;
    if (std::fabs(offset - goal) < kSettleDistance && std::fabs(velocity) < kStopSpeed) {
        offset = goal;
        velocity = 0.0;
        motion = Motion::Resting;
    }
}

bool Scroller::update(float seconds) noexcept {
    const double before = offset;
    switch (motion) {
    case Motion::Flinging: {
        const double decay = std::exp(-kFriction * static_cast<double>(seconds));
        offset += velocity * (1.0 - decay) / kFriction;
        velocity *= decay;
        if (getOverscroll() != 0.0) {
            motion = Motion::Returning;
        } else if (std::fabs(velocity) < kStopSpeed) {
            stop();
        }
        break;
    }
    case Motion::Returning:
        spring(clamp(offset), seconds);
        break;
    case Motion::Animating:
        spring(target, seconds);
        break;
    case Motion::Resting:
    case Motion::Dragging:
        break;
    }
    return offset != before;
}

} // namespace haylen::ui
