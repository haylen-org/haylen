#include "ui/components/touch/PointerTracker.hpp"

#include <optional>

#include "haylen/input/Input.hpp"
#include "haylen/ui/Context.hpp"

namespace haylen::ui {

std::optional<math::Vec2> PointerTracker::update(Context& context, const math::Rect& bounds) {
    const input::Input& devices = context.getInput();
    if (touchId) {
        const input::Touch* touch = devices.findTouch(*touchId);
        if (touch == nullptr || touch->phase == input::TouchPhase::Ended || touch->phase == input::TouchPhase::Cancelled) {
            touchId.reset();
            return std::nullopt;
        }
        return context.toUi(touch->position);
    }
    if (mouseHeld) {
        if (!devices.isMouseDown(input::MouseButton::Left)) {
            mouseHeld = false;
            return std::nullopt;
        }
        return context.toUi(devices.getMousePosition());
    }

    for (const input::Touch& touch : devices.getTouches()) {
        const math::Vec2 position = context.toUi(touch.position);
        if (touch.phase == input::TouchPhase::Began && bounds.contains(position)) {
            touchId = touch.id;
            startPosition = position;
            return position;
        }
    }
    if (devices.getTouches().empty() && devices.isMousePressed(input::MouseButton::Left) && bounds.contains(context.toUi(devices.getMousePosition()))) {
        mouseHeld = true;
        startPosition = context.toUi(devices.getMousePosition());
        return startPosition;
    }
    return std::nullopt;
}

void PointerTracker::reset() noexcept {
    touchId.reset();
    mouseHeld = false;
}

} // namespace haylen::ui
