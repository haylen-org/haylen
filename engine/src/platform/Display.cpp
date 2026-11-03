#include "haylen/platform/Display.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::platform {

const Display::Mode* Display::findMode(int width, int height, float rate) const noexcept {
    constexpr float kRateTolerance = 0.01F;
    const Mode* found = nullptr;
    for (const Mode& mode : modes) {
        if (mode.width != width || mode.height != height) {
            continue;
        }
        if (rate > 0.0F && std::abs(mode.refreshRate - rate) <= kRateTolerance) {
            return &mode;
        }
        if (rate <= 0.0F && (found == nullptr || mode.refreshRate > found->refreshRate)) {
            found = &mode;
        }
    }
    return found;
}

const Display& Display::findHolding(std::span<const Display> displays, const math::Rect& frame) {
    const auto overlap = [&frame](const Display& display) { return display.bounds.intersection(frame).getArea(); };
    const auto largest = std::ranges::max_element(displays, {}, overlap);
    if (overlap(*largest) > 0.0F) {
        return *largest;
    }
    const auto primary = std::ranges::find_if(displays, &Display::primary);
    return primary != displays.end() ? *primary : displays.front();
}

} // namespace haylen::platform
