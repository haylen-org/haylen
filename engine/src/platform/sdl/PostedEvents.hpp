#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <SDL3/SDL_events.h>

#include "haylen/platform/Event.hpp"

namespace haylen::platform::sdl {

// The events that the platform reads on its own, such as those of the page or of the system, each of which waits in the queue of SDL behind a marker that keeps its place among the events of SDL.
class PostedEvents final {
  public:
    // Takes the type of the markers, once the events of SDL started.
    void start();

    // Queues the event behind a marker, and takes it back with an error in the log when the queue of SDL refuses the marker, so no event reaches the app at the marker of another.
    void post(const Event& event);

    [[nodiscard]] bool isMarker(const SDL_Event& event) const noexcept {
        return type != 0 && event.type == type;
    }

    // The event of the marker that SDL handed over, in the order the events were posted.
    [[nodiscard]] Event take();

    [[nodiscard]] std::size_t getWaiting() const noexcept {
        return events.size() - read;
    }

  private:
    std::uint32_t type = 0;
    std::vector<Event> events;
    std::size_t read = 0;
};

} // namespace haylen::platform::sdl
