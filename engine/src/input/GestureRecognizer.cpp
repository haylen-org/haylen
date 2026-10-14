#include "haylen/input/GestureRecognizer.hpp"

#include <cmath>
#include <numbers>

#include "haylen/input/Input.hpp"

namespace haylen::input {

const GestureRecognizer::Settings GestureRecognizer::kDefaultSettings{};

void GestureRecognizer::update(const Input& input, float deltaSeconds) {
    gestures.clear();
    time += deltaSeconds;

    // Long presses fire while the pointer is still down, so a menu can open under the finger.
    // clang-format off
    const auto pressLong = [this](Pointer& pointer) {
        if (!pointer.longPressed && pointer.duration >= settings.longPressDuration && math::Vec2::distance(pointer.start, pointer.position) <= settings.tapMaxMovement) {
            pointer.longPressed = true;
            gestures.push_back({.type = Gesture::Type::LongPress, .position = pointer.position});
        }
    };
    // clang-format on

    // A finger can land and lift within one frame, so a new pointer always starts from where the touch began. Touches keep the order they began in, so an id the platform reuses within one frame ends its old pointer before the new one starts.
    for (const Touch& touch : input.getTouches()) {
        Pointer& pointer = pointers.try_emplace(touch.id, Pointer{.start = touch.startPosition}).first->second;
        pointer.position = touch.position;
        pointer.duration = touch.duration;
        if (touch.isActive()) {
            pressLong(pointer);
            continue;
        }

        // A cancelled touch was taken away from the app, so it never ends as a tap or a swipe.
        if (touch.phase == TouchPhase::Ended) {
            finish(pointer);
        }
        pointers.erase(touch.id);
    }

    if (settings.mouse && input.getTouches().empty()) {
        if (input.isMousePressed(MouseButton::Left)) {
            mouse = Pointer{.start = input.getMousePosition(), .position = input.getMousePosition(), .duration = 0.0F, .longPressed = false};
        } else if (mouse) {
            mouse->position = input.getMousePosition();
            mouse->duration += deltaSeconds;
        }
    }
    if (mouse) {
        if (input.isMouseReleased(MouseButton::Left)) {
            finish(*mouse);
            mouse.reset();
        } else {
            pressLong(*mouse);
        }
    }

    recognizePinch(input);
}

void GestureRecognizer::cancel() noexcept {
    pointers.clear();
    mouse.reset();
    pinchStart.reset();
}

void GestureRecognizer::finish(const Pointer& pointer) {
    if (pointer.shared) {
        return;
    }
    const math::Vec2 delta = pointer.position - pointer.start;
    const float moved = delta.getLength();
    if (!pointer.longPressed && pointer.duration <= settings.tapMaxDuration && moved <= settings.tapMaxMovement) {
        gestures.push_back({.type = Gesture::Type::Tap, .position = pointer.position});
        const bool twice = lastTap && time - lastTap->first <= settings.doubleTapInterval && math::Vec2::distance(lastTap->second, pointer.position) <= settings.doubleTapDistance;
        if (twice) {
            gestures.push_back({.type = Gesture::Type::DoubleTap, .position = pointer.position});
            lastTap.reset();
        } else {
            lastTap = std::pair{time, pointer.position};
        }
        return;
    }
    if (moved >= settings.swipeMinDistance && pointer.duration <= settings.swipeMaxDuration) {
        gestures.push_back({.type = Gesture::Type::Swipe, .position = pointer.position, .delta = delta});
    }
}

// The list of the fingers down keeps its memory from frame to frame.
void GestureRecognizer::recognizePinch(const Input& input) {
    fingers.clear();
    for (const Touch& touch : input.getTouches()) {
        if (touch.isActive()) {
            fingers.push_back(&touch);
        }
    }
    // Fingers that ever shared the screen belong to that gesture and never end as taps or swipes.
    if (fingers.size() > 1) {
        for (const Touch* finger : fingers) {
            pointers[finger->id].shared = true;
        }
    }
    if (fingers.size() != 2) {
        pinchStart.reset();
        return;
    }

    // The finger that touched first leads, so the angle of the pinch never flips with the order the platform lists the touches in.
    const Touch& lead = fingers[0]->id < fingers[1]->id ? *fingers[0] : *fingers[1];
    const Touch& other = &lead == fingers[0] ? *fingers[1] : *fingers[0];
    const math::Vec2 between = other.position - lead.position;
    const float spread = between.getLength();
    const float angle = between.getAngle();
    const math::Vec2 center = (lead.position + other.position) * 0.5F;
    if (!pinchStart) {
        pinchStart = PinchStart{.spread = spread, .angle = angle, .center = center};
    }
    const bool moved = lead.phase == TouchPhase::Moved || other.phase == TouchPhase::Moved;
    if (moved && pinchStart->spread > 0.0F) {
        const float rotation = std::remainder(angle - pinchStart->angle, 2.0F * std::numbers::pi_v<float>);
        gestures.push_back({.type = Gesture::Type::Pinch, .position = center, .delta = center - pinchStart->center, .scale = spread / pinchStart->spread, .rotation = rotation});
    }
}

} // namespace haylen::input
