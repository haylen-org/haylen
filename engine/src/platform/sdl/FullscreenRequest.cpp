#include "platform/sdl/FullscreenRequest.hpp"

#include <utility>

namespace haylen::platform::sdl {

// The window closes with the request, which never reaches its mode then.
FullscreenRequest::~FullscreenRequest() {
    settle({.state = SystemAnswer::State::Denied, .message = "The window closed before it finished the change of fullscreen."});
}

void FullscreenRequest::begin(bool fullscreen, Callback callback) {
    settle({.state = SystemAnswer::State::Denied, .message = "A newer change of fullscreen replaced this one before the window finished it."});
    wanted = fullscreen;
    waiting = std::move(callback);
}

void FullscreenRequest::beginLeaving(Destination value, Callback callback) {
    begin(true, std::move(callback));
    destination = std::move(value);
}

FullscreenRequest::Destination FullscreenRequest::takeDestination() {
    Destination taken = std::move(destination.value());
    destination.reset();
    return taken;
}

// The window reaches the fullscreen of the request only on its destination, so a request that waits for the window to leave answers nothing yet.
void FullscreenRequest::reach(bool fullscreen) {
    if (wanted != fullscreen || destination) {
        return;
    }

    settle({.state = SystemAnswer::State::Granted});
}

void FullscreenRequest::fail(SystemAnswer answer) {
    settle(std::move(answer));
}

void FullscreenRequest::settle(SystemAnswer answer) {
    if (!wanted) {
        return;
    }

    wanted.reset();
    destination.reset();
    const Callback callback = std::exchange(waiting, {});
    callback(std::move(answer));
}

} // namespace haylen::platform::sdl
