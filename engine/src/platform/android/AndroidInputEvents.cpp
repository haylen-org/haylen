#include "platform/android/AndroidInputEvents.hpp"

#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidMouseWheel.hpp"
#include "sokol_app.h"

namespace haylen::platform {

bool AndroidInputEvents::take(const void* source) {
    if (AndroidGamepads::handleEvent(source)) {
        return true;
    }
    const auto& event = *static_cast<const sapp_android_native_event*>(source);
    return event.type == SAPP_ANDROID_NATIVE_EVENT_MOTION && AndroidMouseWheel::handleEvent(*static_cast<const GameActivityMotionEvent*>(event.event));
}

} // namespace haylen::platform
