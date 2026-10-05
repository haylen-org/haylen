#include "haylen/platform/Fold.hpp"

#include <algorithm>

namespace haylen::platform {

std::string_view Fold::axisName(Axis value) noexcept {
    return std::ranges::find(kAxisNames, value, &std::pair<std::string_view, Axis>::second)->first;
}

std::string_view Fold::stateName(State value) noexcept {
    return std::ranges::find(kStateNames, value, &std::pair<std::string_view, State>::second)->first;
}

std::string_view Fold::postureName(Posture value) noexcept {
    return std::ranges::find(kPostureNames, value, &std::pair<std::string_view, Posture>::second)->first;
}

Fold::Posture Fold::getPosture() const noexcept {
    if (state != State::HalfOpened) {
        return Posture::Flat;
    }
    return axis == Axis::Horizontal ? Posture::Tabletop : Posture::Book;
}

std::vector<math::Rect> Fold::getSegments(math::Vec2 size) const {
    if (!separating) {
        return {{0.0F, 0.0F, size.x, size.y}};
    }
    if (axis == Axis::Vertical) {
        const float start = std::clamp(bounds.getLeft(), 0.0F, size.x);
        const float end = std::clamp(bounds.getRight(), start, size.x);
        return {{0.0F, 0.0F, start, size.y}, {end, 0.0F, size.x - end, size.y}};
    }
    const float start = std::clamp(bounds.getTop(), 0.0F, size.y);
    const float end = std::clamp(bounds.getBottom(), start, size.y);
    return {{0.0F, 0.0F, size.x, start}, {0.0F, end, size.x, size.y - end}};
}

} // namespace haylen::platform
