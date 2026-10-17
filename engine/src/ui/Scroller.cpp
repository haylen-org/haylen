#include "ui/Scroller.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::ui {

double Scroller::clamp(double value) const noexcept {
    return std::clamp(value, 0.0, maximum);
}

// Content stretches past an end the way a rubber band does: by the pull while the pull is short, and toward the length of the view as the pull grows.
double Scroller::stretch(double pull) const noexcept {
    if (view <= 0.0) {
        return 0.0;
    }
    return std::copysign(view * (1.0 - 1.0 / (std::fabs(pull) * kElasticity / view + 1.0)), pull);
}

double Scroller::unstretch(double overscroll) const noexcept {
    if (view <= 0.0) {
        return 0.0;
    }
    const double reach = std::min(std::fabs(overscroll), view * 0.999);
    return std::copysign(reach * view / (kElasticity * (view - reach)), overscroll);
}

void Scroller::pull(double delta) noexcept {
    const double inside = clamp(offset);
    const double reach = inside + unstretch(offset - inside) + delta;
    const double edge = clamp(reach);
    offset = edge + stretch(reach - edge);
}

void Scroller::setRange(double maximumValue, double viewLength) noexcept {
    maximum = std::max(0.0, maximumValue);
    view = std::max(0.0, viewLength);
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
    // The speed follows the content, which moves less than the finger past an end, so a release there springs back without flinging further out.
    const double before = offset;
    pull(delta);
    if (seconds > 0.0F) {
        velocity = velocity * 0.4 + (offset - before) / static_cast<double>(seconds) * 0.6;
    }
}

void Scroller::follow(const Context::WheelScroll& wheel, double toward, float seconds) noexcept {
    if (wheel.touched && motion == Motion::Flinging) {
        stop();
    }

    const double delta = toward * static_cast<double>(wheel.distance);
    switch (wheel.motion) {
    case Context::WheelScroll::Motion::Glide:
        animateTo((motion == Motion::Animating ? target : clamp(offset)) + delta);
        break;
    case Context::WheelScroll::Motion::Drag:
        // Fingers that leave without moving keep the speed they had, which the release flings with.
        if (delta != 0.0 || !wheel.lifted) {
            drag(delta, seconds);
        }
        break;
    case Context::WheelScroll::Motion::Free:
        jumpTo(offset + delta);
        break;
    case Context::WheelScroll::Motion::None:
        break;
    }

    if (wheel.lifted && motion == Motion::Dragging) {
        release();
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
