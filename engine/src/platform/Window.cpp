#include "haylen/platform/Window.hpp"

#include <algorithm>

namespace haylen::platform {

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
