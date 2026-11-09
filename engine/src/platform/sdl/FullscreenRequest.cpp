#include "platform/sdl/FullscreenRequest.hpp"

#include <utility>

namespace haylen::platform::sdl {

// The window closes with the request, which never reaches its mode then.
FullscreenRequest::~FullscreenRequest() {
    settle({.state = Permission::State::Denied, .message = "The window closed before it finished the change of fullscreen."});
}

void FullscreenRequest::begin(bool fullscreen, Callback callback) {
    settle({.state = Permission::State::Denied, .message = "A newer change of fullscreen replaced this one before the window finished it."});
    wanted = fullscreen;
    waiting = std::move(callback);
}

void FullscreenRequest::reach(bool fullscreen) {
    if (wanted != fullscreen) {
        return;
    }
    settle({.state = Permission::State::Granted});
}

void FullscreenRequest::fail(Permission::Answer answer) {
    settle(std::move(answer));
}

void FullscreenRequest::settle(Permission::Answer answer) {
    if (!wanted) {
        return;
    }
    wanted.reset();
    const Callback callback = std::exchange(waiting, {});
    callback(std::move(answer));
}

} // namespace haylen::platform::sdl
