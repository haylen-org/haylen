#include "haylen/ui/Scaling.hpp"

#include <algorithm>

namespace haylen::ui {

std::optional<Scaling::Mode> Scaling::modeFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kModes, name, &std::pair<std::string_view, Mode>::first);
    return found != kModes.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Scaling::modeName(Mode value) noexcept {
    return std::ranges::find(kModes, value, &std::pair<std::string_view, Mode>::second)->first;
}

float Scaling::resolve(float pixelsPerUnit, float pixelsPerPoint, math::Vec2 visibleSize) const noexcept {
    if (mode == Mode::Design || pixelsPerUnit <= 0.0F) {
        return factor;
    }
    const float physical = pixelsPerPoint * kPointsPerUnit * factor / pixelsPerUnit;
    return std::min(physical, std::min(visibleSize.x, visibleSize.y) / kMinimumShortSide);
}

} // namespace haylen::ui
