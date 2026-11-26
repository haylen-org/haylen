#pragma once

#include <string>
#include <vector>

#include <SDL3/SDL_properties.h>
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

    // The id of a display from its properties, the name of its output, such as `DP-1`. Throws `std::runtime_error` when the compositor names no output.
    [[nodiscard]] static std::string readId(SDL_PropertiesID properties);

    // The pixels in a point of an output, which SDL keeps in the pixel density of its desktop mode, since it gives a content scale only to the windows that it scales to their display.
    [[nodiscard]] static float readScale(const SDL_DisplayMode& desktop) noexcept;

  private:
    [[nodiscard]] static Display describe(SDL_DisplayID id, SDL_DisplayID primary);
    [[nodiscard]] static std::string getId(SDL_DisplayID id);
    [[nodiscard]] static std::string getName(SDL_DisplayID id);
};

} // namespace haylen::platform::sdl
