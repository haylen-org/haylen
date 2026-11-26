#include "platform/sdl/PostedEvents.hpp"

#include "haylen/core/Log.hpp"

namespace haylen::platform::sdl {

void PostedEvents::start() {
    type = SDL_RegisterEvents(1);
}

void PostedEvents::post(const Event& event) {
    events.push_back(event);
    SDL_Event marker{};
    marker.type = type;
    if (!SDL_PushEvent(&marker)) {
        events.pop_back();
        core::Log::error("An event of the platform could not reach the app, since the queue of the events of the window is full.");
    }
}

// The events go once the app took every one of them, so the storage of the next ones reuses theirs.
Event PostedEvents::take() {
    const Event event = events[read++];
    if (read == events.size()) {
        events.clear();
        read = 0;
    }

    return event;
}

} // namespace haylen::platform::sdl
