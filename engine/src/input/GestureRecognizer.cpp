#include "haylen/input/GestureRecognizer.hpp"

#include "haylen/input/Input.hpp"

namespace haylen::input {

const GestureRecognizer::Settings GestureRecognizer::kDefaultSettings{};

bool GestureRecognizer::isActive(const Touch& touch) noexcept {
    return touch.phase != TouchPhase::Ended && touch.phase != TouchPhase::Cancelled;
}

void GestureRecognizer::update(const Input& input, float deltaSeconds) {
    gestures.clear();
    time += deltaSeconds;

    // A finger can land and lift within one frame, so a new pointer always starts from where the touch began.
    for (const Touch& touch : input.getTouches()) {
        Pointer& pointer = pointers.try_emplace(touch.id, Pointer{.start = touch.startPosition}).first->second;
        pointer.position = touch.position;
        pointer.duration = touch.duration;
    }
    if (settings.mouse && input.getTouches().empty()) {
        if (input.isMousePressed(MouseButton::Left)) {
            mouse = Pointer{.start = input.getMousePosition(), .position = input.getMousePosition(), .duration = 0.0F, .longPressed = false};
        } else if (mouse) {
            mouse->position = input.getMousePosition();
            mouse->duration += deltaSeconds;
        }
    }

    // Long presses fire while the pointer is still down, so a menu can open under the finger.
    // clang-format off
    const auto pressLong = [this](Pointer& pointer) {
        if (!pointer.longPressed && pointer.duration >= settings.longPressDuration && math::Vec2::distance(pointer.start, pointer.position) <= settings.tapMaxMovement) {
            pointer.longPressed = true;
            gestures.push_back({.type = Gesture::Type::LongPress, .position = pointer.position});
        }
    };
    // clang-format on

    for (const Touch& touch : input.getTouches()) {
        Pointer& pointer = pointers[touch.id];
        if (isActive(touch)) {
            pressLong(pointer);
            continue;
        }
        finish(pointer);
        pointers.erase(touch.id);
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

void GestureRecognizer::recognizePinch(const Input& input) {
    std::vector<const Touch*> fingers;
    for (const Touch& touch : input.getTouches()) {
        if (isActive(touch)) {
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

    const float spread = math::Vec2::distance(fingers[0]->position, fingers[1]->position);
    if (!pinchStart) {
        pinchStart = spread;
    }
    const bool moved = fingers[0]->phase == TouchPhase::Moved || fingers[1]->phase == TouchPhase::Moved;
    if (moved && *pinchStart > 0.0F) {
        const math::Vec2 center = (fingers[0]->position + fingers[1]->position) * 0.5F;
        gestures.push_back({.type = Gesture::Type::Pinch, .position = center, .scale = spread / *pinchStart});
    }
}

} // namespace haylen::input
