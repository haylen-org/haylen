#include "haylen/platform/Window.hpp"

#include <algorithm>

namespace haylen::platform {

const std::array<std::pair<std::string_view, Orientation>, 3> Window::kOrientationNames{{{"landscape", Orientation::Landscape}, {"portrait", Orientation::Portrait}, {"any", Orientation::Any}}};
const std::array<std::pair<std::string_view, Window::StatusBar>, 4> Window::kStatusBarNames{{{"hidden", StatusBar::Hidden}, {"auto", StatusBar::Auto}, {"light", StatusBar::Light}, {"dark", StatusBar::Dark}}};
const std::array<std::pair<std::string_view, Window::SystemGestures>, 3> Window::kSystemGesturesNames{{{"system", SystemGestures::System}, {"deferred", SystemGestures::Deferred}, {"autoHideIndicator", SystemGestures::AutoHideIndicator}}};

std::optional<Orientation> Window::orientationFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kOrientationNames, name, &std::pair<std::string_view, Orientation>::first);
    return found != kOrientationNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Window::orientationName(Orientation value) noexcept {
    return std::ranges::find(kOrientationNames, value, &std::pair<std::string_view, Orientation>::second)->first;
}

std::optional<Window::StatusBar> Window::statusBarFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kStatusBarNames, name, &std::pair<std::string_view, StatusBar>::first);
    return found != kStatusBarNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Window::statusBarName(StatusBar value) noexcept {
    return std::ranges::find(kStatusBarNames, value, &std::pair<std::string_view, StatusBar>::second)->first;
}

std::optional<Window::SystemGestures> Window::systemGesturesFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kSystemGesturesNames, name, &std::pair<std::string_view, SystemGestures>::first);
    return found != kSystemGesturesNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Window::systemGesturesName(SystemGestures value) noexcept {
    return std::ranges::find(kSystemGesturesNames, value, &std::pair<std::string_view, SystemGestures>::second)->first;
}

} // namespace haylen::platform
