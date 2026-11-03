#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "haylen/platform/Display.hpp"
#include "haylen/platform/Window.hpp"

namespace haylen::platform {

// The display and the exact mode that a fullscreen of `Window::enterFullscreen` asks for.
struct FullscreenTarget {
    Display display;
    std::optional<Display::Mode> mode;

    // Finds the display with the id of the request, or the display of the window while the request names none, and the mode of the display that the request asks for. Throws `std::invalid_argument` for a display that is not connected or a mode that the display does not list.
    [[nodiscard]] static FullscreenTarget resolve(std::span<const Display> displays, std::string_view windowDisplay, const Window::Fullscreen& request);
};

} // namespace haylen::platform
