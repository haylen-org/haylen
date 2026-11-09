#pragma once

#include <string>
#include <vector>

#include <SDL3/SDL_video.h>

#include "haylen/platform/Display.hpp"

namespace haylen::platform::sdl {

// The displays of a Wayland session, which the compositor keeps to itself and SDL reads through the outputs that Wayland announces. Each display takes the name of its output as its id, and its bounds and work area in the points of the desktop, which SDL measures them in.
class WaylandDisplays final {
  public:
    // Whether the window system of SDL is Wayland, where the services of the desktop, which read the displays through X11, have no server to ask.
    [[nodiscard]] static bool isSession() noexcept;

    // Throws `std::runtime_error` with the reason of SDL when it lists no display.
    [[nodiscard]] static std::vector<Display> read();
    [[nodiscard]] static std::string findWindowDisplay(SDL_Window* window);

    // Returns the SDL display of the id. Throws `std::runtime_error` when no display of SDL has that id.
    [[nodiscard]] static SDL_DisplayID find(const Display& display);

  private:
    [[nodiscard]] static Display describe(SDL_DisplayID id, SDL_DisplayID primary);
    [[nodiscard]] static std::string getId(SDL_DisplayID id);
};

} // namespace haylen::platform::sdl
