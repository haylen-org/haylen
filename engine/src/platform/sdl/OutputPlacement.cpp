#include "platform/sdl/OutputPlacement.hpp"

namespace haylen::platform::sdl {

bool OutputPlacement::take(const SDL_Event& event) noexcept {
    if (placed || event.type != SDL_EVENT_WINDOW_ENTER_FULLSCREEN) {
        return false;
    }

    placed = true;
    return true;
}

} // namespace haylen::platform::sdl
