#include "haylen/input/PointerEmulation.hpp"

#include <algorithm>

namespace haylen::input {

void PointerEmulation::setMouseAsTouch(bool value) noexcept {
    mouseAsTouch = value;
    mouseFingerDown = false;
}

void PointerEmulation::setTouchAsMouse(bool value) noexcept {
    touchAsMouse = value;
    mouseFinger.reset();
}

std::span<const platform::Event> PointerEmulation::convert(const platform::Event& event) {
    count = 0;
    switch (event.type) {
    case platform::Event::Type::MouseDown:
    case platform::Event::Type::MouseUp:
    case platform::Event::Type::MouseMove:
        if (mouseAsTouch) {
            return convertMouse(event);
        }
        break;
    case platform::Event::Type::TouchBegan:
    case platform::Event::Type::TouchMoved:
    case platform::Event::Type::TouchEnded:
    case platform::Event::Type::TouchCancelled:
        if (touchAsMouse) {
            return convertTouch(event);
        }
        break;
    default:
        break;
    }
    return {&event, 1};
}

// The left button and the moves while it is held become one finger, the moves without a button vanish as touch screens have no hover, and the other buttons stay mouse buttons.
std::span<const platform::Event> PointerEmulation::convertMouse(const platform::Event& event) {
    if (event.type != platform::Event::Type::MouseMove && event.mouseButton != MouseButton::Left) {
        return {&event, 1};
    }
    platform::Event::Type type = platform::Event::Type::TouchMoved;
    if (event.type == platform::Event::Type::MouseDown) {
        type = platform::Event::Type::TouchBegan;
        mouseFingerDown = true;
    } else if (!mouseFingerDown) {
        return {};
    } else if (event.type == platform::Event::Type::MouseUp) {
        type = platform::Event::Type::TouchEnded;
        mouseFingerDown = false;
    }

    platform::Event& touch = converted[count++];
    touch = platform::Event{};
    touch.type = type;
    touch.modifiers = event.modifiers;
    touch.touchCount = 1;
    touch.touches[0] = {.id = kMouseTouchId, .position = event.position, .changed = true};
    return {converted.data(), count};
}

// The first finger down drives the mouse until it lifts, and its mouse events come before the touch event, so the touch screen stays the last device the app used.
std::span<const platform::Event> PointerEmulation::convertTouch(const platform::Event& event) {
    const std::span<const platform::TouchPoint> points(event.touches.data(), event.touchCount);
    const auto changed = [](const platform::TouchPoint& point) { return point.changed; };
    if (!mouseFinger && event.type == platform::Event::Type::TouchBegan) {
        if (const auto first = std::ranges::find_if(points, changed); first != points.end()) {
            mouseFinger = first->id;
            addMouse(platform::Event::Type::MouseMove, first->position);
            addMouse(platform::Event::Type::MouseDown, first->position);
        }
    } else if (mouseFinger) {
        const auto finger = std::ranges::find_if(points, [this](const platform::TouchPoint& point) { return point.changed && point.id == *mouseFinger; });
        if (finger != points.end() && event.type != platform::Event::Type::TouchBegan) {
            addMouse(platform::Event::Type::MouseMove, finger->position);
            if (event.type != platform::Event::Type::TouchMoved) {
                addMouse(platform::Event::Type::MouseUp, finger->position);
                mouseFinger.reset();
            }
        }
    }
    converted[count++] = event;
    return {converted.data(), count};
}

void PointerEmulation::addMouse(platform::Event::Type type, math::Vec2 position) {
    platform::Event& mouse = converted[count++];
    mouse = platform::Event{};
    mouse.type = type;
    mouse.mouseButton = MouseButton::Left;
    mouse.position = position;
}

} // namespace haylen::input
