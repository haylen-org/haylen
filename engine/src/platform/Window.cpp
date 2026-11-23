#include "haylen/platform/Window.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string>
#include <utility>

namespace haylen::platform {

const std::array<std::pair<std::string_view, Orientation>, 3> Window::kOrientationNames{{{"landscape", Orientation::Landscape}, {"portrait", Orientation::Portrait}, {"any", Orientation::Any}}};
const std::array<std::pair<std::string_view, Window::StatusBar>, 4> Window::kStatusBarNames{{{"hidden", StatusBar::Hidden}, {"auto", StatusBar::Auto}, {"light", StatusBar::Light}, {"dark", StatusBar::Dark}}};
const std::array<std::pair<std::string_view, Window::SystemGestures>, 3> Window::kSystemGesturesNames{{{"system", SystemGestures::System}, {"deferred", SystemGestures::Deferred}, {"autoHideIndicator", SystemGestures::AutoHideIndicator}}};

const std::array<std::pair<std::string_view, Window::Redraw>, 2> Window::kRedrawNames{{{"always", Redraw::Always}, {"onChange", Redraw::OnChange}}};

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

void Window::requireFrameRate(int value) {
    if (value < 0 || value > kMaxFrameRate) {
        throw std::invalid_argument("The frame rate " + std::to_string(value) + " is out of range. It is 0 for every refresh of the display, or from 1 to " + std::to_string(kMaxFrameRate) + " frames per second.");
    }
}

void Window::requireMaxPixelDensity(float value) {
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument(std::format("The pixel density cap {} is out of range. It is 0 for every pixel of the display, or a positive number of framebuffer pixels in each point.", value));
    }
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

std::optional<Window::Redraw> Window::redrawFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kRedrawNames, name, &std::pair<std::string_view, Redraw>::first);
    return found != kRedrawNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Window::redrawName(Redraw value) noexcept {
    return std::ranges::find(kRedrawNames, value, &std::pair<std::string_view, Redraw>::second)->first;
}

// An empty callback stands for a caller that does not want the answer, so the window answers nobody.
std::function<void(SystemAnswer answer)> Window::answerTo(std::function<void(SystemAnswer answer)> callback) {
    if (callback) {
        return callback;
    }

    return [](const SystemAnswer&) {};
}

void Window::setFullscreen(bool value, std::function<void(SystemAnswer answer)> callback) {
    applyFullscreen(value, answerTo(std::move(callback)));
}

void Window::enterFullscreen(const Fullscreen& value, std::function<void(SystemAnswer answer)> callback) {
    applyFullscreenTarget(value, answerTo(std::move(callback)));
}

void Window::setMouseLocked(bool value, std::function<void(SystemAnswer answer)> callback) {
    applyMouseLocked(value, answerTo(std::move(callback)));
}

void Window::setKeepScreenOn(bool value, std::function<void(SystemAnswer answer)> callback) {
    applyKeepScreenOn(value, answerTo(std::move(callback)));
}

} // namespace haylen::platform
