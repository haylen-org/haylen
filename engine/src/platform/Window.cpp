#include "haylen/platform/Window.hpp"

#include <algorithm>

namespace haylen::platform {

const std::array<std::pair<std::string_view, Orientation>, 3> Window::kOrientationNames{{{"landscape", Orientation::Landscape}, {"portrait", Orientation::Portrait}, {"any", Orientation::Any}}};

std::optional<Orientation> Window::orientationFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kOrientationNames, name, &std::pair<std::string_view, Orientation>::first);
    return found != kOrientationNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Window::orientationName(Orientation value) noexcept {
    return std::ranges::find(kOrientationNames, value, &std::pair<std::string_view, Orientation>::second)->first;
}

Monitor Window::getCurrentMonitor() const {
    const std::vector<Monitor> monitors = getMonitors();
    const math::Rect frame = getFrame();
    const auto overlap = [&frame](const Monitor& monitor) { return monitor.bounds.intersection(frame).getArea(); };
    const auto largest = std::ranges::max_element(monitors, {}, overlap);
    if (overlap(*largest) > 0.0F) {
        return *largest;
    }
    const auto primary = std::ranges::find_if(monitors, &Monitor::primary);
    return primary != monitors.end() ? *primary : monitors.front();
}

} // namespace haylen::platform
