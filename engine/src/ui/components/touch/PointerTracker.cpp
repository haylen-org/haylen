#include "ui/components/touch/PointerTracker.hpp"

#include <optional>

#include "haylen/input/Input.hpp"
#include "haylen/ui/Context.hpp"

namespace haylen::ui {

bool PointerTracker::isLifted(const input::Touch& touch) noexcept {
    return touch.phase == input::TouchPhase::Ended || touch.phase == input::TouchPhase::Cancelled;
}

std::optional<math::Vec2> PointerTracker::update(Context& context, const math::Rect& bounds, bool slideIn) {
    const input::Input& devices = context.getInput();
    if (touchId) {
        const input::Touch* touch = devices.findTouch(*touchId);
        const bool left = touch == nullptr || isLifted(*touch) || (slideIn && !bounds.contains(context.toUi(touch->position)));
        if (!left) {
            return context.toUi(touch->position);
        }
        touchId.reset();
        if (!slideIn) {
            return std::nullopt;
        }
    }
    if (mouseHeld) {
        const bool left = !devices.isMouseDown(input::MouseButton::Left) || (slideIn && !bounds.contains(context.toUi(devices.getMousePosition())));
        if (!left) {
            return context.toUi(devices.getMousePosition());
        }
        mouseHeld = false;
        if (!slideIn) {
            return std::nullopt;
        }
    }

    // A finger that lands on the control takes it, and with sliding fingers so does one that is down anywhere and moves onto it.
    for (const input::Touch& touch : devices.getTouches()) {
        const math::Vec2 position = context.toUi(touch.position);
        const bool takes = slideIn ? !isLifted(touch) : touch.phase == input::TouchPhase::Began;
        if (takes && bounds.contains(position)) {
            touchId = touch.id;
            startPosition = position;
            return position;
        }
    }
    const bool mouseTakes = slideIn ? devices.isMouseDown(input::MouseButton::Left) : devices.isMousePressed(input::MouseButton::Left);
    if (devices.getTouches().empty() && mouseTakes && bounds.contains(context.toUi(devices.getMousePosition()))) {
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
