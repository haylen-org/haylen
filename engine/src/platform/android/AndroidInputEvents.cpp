#include "platform/android/AndroidInputEvents.hpp"

#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidMouseWheel.hpp"

namespace haylen::platform {

bool AndroidInputEvents::takeKey(const GameActivityKeyEvent& event) {
    return AndroidGamepads::handleKey(event);
}

bool AndroidInputEvents::takeMotion(const GameActivityMotionEvent& event) {
    return AndroidGamepads::handleMotion(event) || AndroidMouseWheel::handleEvent(event);
}

bool AndroidInputEvents::takesKey(const GameActivityKeyEvent& event) {
    return AndroidGamepads::takesKey(event);
}

} // namespace haylen::platform
