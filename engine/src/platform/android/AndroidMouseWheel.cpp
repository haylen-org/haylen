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

} // namespace haylen::platform
