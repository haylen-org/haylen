#include "platform/android/AndroidMouseWheel.hpp"

#include <android/input.h>

#include "platform/NativeWindow.hpp"
#include "platform/Player.hpp"

namespace haylen::platform {

void AndroidMouseWheel::enableAxes() {
    GameActivityPointerAxes_enableAxis(AMOTION_EVENT_AXIS_VSCROLL);
    GameActivityPointerAxes_enableAxis(AMOTION_EVENT_AXIS_HSCROLL);
}

// Android counts a step of a wheel as one, scrolling up and to the right with positive values, and places the pointer in pixels of the window, which the scale of the window turns into framebuffer pixels.
bool AndroidMouseWheel::handleEvent(const GameActivityMotionEvent& event) {
    if ((event.action & AMOTION_EVENT_ACTION_MASK) != AMOTION_EVENT_ACTION_SCROLL || event.pointerCount < 1) {
        return false;
    }

    const GameActivityPointerAxes& axes = event.pointers[0];
    const float scale = NativeWindow::getScale();
    const math::Vec2 position{GameActivityPointerAxes_getAxisValue(&axes, AMOTION_EVENT_AXIS_X) * scale, GameActivityPointerAxes_getAxisValue(&axes, AMOTION_EVENT_AXIS_Y) * scale};
    Player::handleEvent({.type = Event::Type::MouseMove, .position = position});
    Player::handleEvent({.type = Event::Type::MouseScroll, .scroll = {-GameActivityPointerAxes_getAxisValue(&axes, AMOTION_EVENT_AXIS_HSCROLL), GameActivityPointerAxes_getAxisValue(&axes, AMOTION_EVENT_AXIS_VSCROLL)}});
    return true;
}

// The phases come in the order of `Event::ScrollPhase`, from `HaylenTouchpadSwipe`, and the host turns the place into framebuffer pixels on the frame thread.
void AndroidMouseWheel::receiveTouchpadScroll(int phase, float x, float y, float deltaX, float deltaY) {
    const auto scrollPhase = static_cast<Event::ScrollPhase>(phase);
    const math::Vec2 position{x, y};
    Player::postEvent({.type = Event::Type::MouseMove, .position = position});
    Player::postEvent({.type = Event::Type::MouseScroll, .position = position, .scroll = {deltaX, deltaY}, .preciseScroll = true, .scrollPhase = scrollPhase});
}

} // namespace haylen::platform
