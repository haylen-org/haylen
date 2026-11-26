#pragma once

#include <SDL3/SDL_events.h>

namespace haylen::platform::sdl {

// Decides when the window of an output reached its place on its display, before which its output draws into it and announces nothing. A window of the chosen size is in place at once, and a window that covers its display once SDL reports it in fullscreen, since a window system may move and size the window a moment after it opened, as macOS, X11 and Wayland do, and Wayland only once the window showed a frame, so the output never announces the size and the scale of the window before it got there.
class OutputPlacement final {
  public:
    explicit OutputPlacement(bool fullscreen) noexcept : placed(!fullscreen) {}

    // Takes an event of the window, and returns whether the window reached its place with it.
    [[nodiscard]] bool take(const SDL_Event& event) noexcept;

    [[nodiscard]] bool isPlaced() const noexcept {
        return placed;
    }

  private:
    bool placed;
};

} // namespace haylen::platform::sdl
