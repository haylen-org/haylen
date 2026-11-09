#include "platform/sdl/WaylandDisplays.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_stdinc.h>

namespace haylen::platform::sdl {

bool WaylandDisplays::isSession() noexcept {
    const char* driver = SDL_GetCurrentVideoDriver();
    return driver != nullptr && std::string_view(driver) == "wayland";
}

std::vector<Display> WaylandDisplays::read() {
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    if (ids == nullptr || count == 0) {
        SDL_free(ids);
        throw std::runtime_error("SDL lists no display. " + std::string(SDL_GetError()));
    }

    const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    std::vector<Display> displays;
    displays.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        displays.push_back(describe(ids[index], primary));
    }
    SDL_free(ids);
    return displays;
}

std::string WaylandDisplays::findWindowDisplay(SDL_Window* window) {
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (display == 0) {
        throw std::runtime_error("SDL knows no display that shows the window. " + std::string(SDL_GetError()));
    }
    return getId(display);
}

SDL_DisplayID WaylandDisplays::find(const Display& display) {
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    SDL_DisplayID found = 0;
    for (int index = 0; index < count && found == 0; ++index) {
        if (getId(ids[index]) == display.id) {
            found = ids[index];
        }
    }
    SDL_free(ids);
    if (found == 0) {
        throw std::runtime_error("SDL knows no display \"" + display.id + "\".");
    }
    return found;
}

// The bounds and the work area are in the points of the desktop, and the modes in pixels at their own density.
Display WaylandDisplays::describe(SDL_DisplayID id, SDL_DisplayID primary) {
    SDL_Rect bounds{};
    SDL_Rect usable{};
    if (!SDL_GetDisplayBounds(id, &bounds) || !SDL_GetDisplayUsableBounds(id, &usable)) {
        throw std::runtime_error("SDL could not read the bounds of a display. " + std::string(SDL_GetError()));
    }
    const float scale = SDL_GetDisplayContentScale(id);
    Display display{
        .id = getId(id),
        .name = getId(id),
        .bounds = {static_cast<float>(bounds.x), static_cast<float>(bounds.y), static_cast<float>(bounds.w), static_cast<float>(bounds.h)},
        .workArea = {static_cast<float>(usable.x), static_cast<float>(usable.y), static_cast<float>(usable.w), static_cast<float>(usable.h)},
        .scale = scale > 0.0F ? scale : 1.0F,
        .primary = id == primary,
    };
    if (const SDL_DisplayMode* current = SDL_GetCurrentDisplayMode(id)) {
        display.refreshRate = current->refresh_rate;
    }

    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(id, &count);
    for (int index = 0; index < count; ++index) {
        const SDL_DisplayMode& mode = *modes[index];
        const Display::Mode found{.width = mode.w, .height = mode.h, .refreshRate = mode.refresh_rate, .scale = mode.pixel_density};
        if (std::ranges::find(display.modes, found) == display.modes.end()) {
            display.modes.push_back(found);
        }
    }
    SDL_free(static_cast<void*>(modes));
    return display;
}

// The name of a Wayland output, such as `DP-1`, stays the same while the monitor stays on its connector.
std::string WaylandDisplays::getId(SDL_DisplayID id) {
    const char* name = SDL_GetDisplayName(id);
    if (name == nullptr) {
        throw std::runtime_error("SDL could not read the name of a display. " + std::string(SDL_GetError()));
    }
    return name;
}

} // namespace haylen::platform::sdl
