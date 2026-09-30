#include "platform/android/AndroidKeys.hpp"

#include "haylen/platform/Event.hpp"
#include "platform/sokol/SokolRuntime.hpp"

namespace haylen::platform {

void AndroidKeys::receiveBack() {
    SokolRuntime::postEvent({.type = Event::Type::KeyDown, .key = input::Key::Escape});
    SokolRuntime::postEvent({.type = Event::Type::KeyUp, .key = input::Key::Escape});
}

} // namespace haylen::platform
