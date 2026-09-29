#include "haylen/ui/Anchor.hpp"

#include <algorithm>
#include <cmath>

#include "haylen/ui/Component.hpp"

namespace haylen::ui {

std::optional<Anchor> Anchor::fromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kPresets, name, &Preset::name);
    if (found == kPresets.end()) {
        return std::nullopt;
    }
    return Anchor{.horizontal = found->horizontal, .vertical = found->vertical};
}

math::Rect Anchor::place(math::Vec2 size, const math::Rect& area) const noexcept {
    const float width = horizontal == Alignment::Stretch ? area.width : std::min(size.x, area.width);
    const float height = vertical == Alignment::Stretch ? area.height : std::min(size.y, area.height);
    return {std::floor(Component::align(horizontal, area.x, area.width, width)), std::floor(Component::align(vertical, area.y, area.height, height)), width, height};
}

} // namespace haylen::ui
