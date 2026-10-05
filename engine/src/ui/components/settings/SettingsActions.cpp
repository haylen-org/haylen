#include "ui/components/settings/SettingsActions.hpp"

#include <algorithm>
#include <ranges>

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

math::Vec2 SettingsActions::measureContent(Context& context, float availableWidth) {
    math::Vec2 size;
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    for (Component* child : getLayoutChildren()) {
        const math::Vec2 measured = child->measure(context, availableWidth);
        size = {size.x + measured.x + spacing, std::max(size.y, measured.y)};
    }
    return {std::max(0.0F, size.x - spacing), size.y + spacing};
}

void SettingsActions::render(Context& context, const math::Rect& bounds) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    float x = bounds.getRight();
    for (Component* child : getLayoutChildren() | std::views::reverse) {
        const math::Vec2 size = child->measure(context, bounds.width);
        x -= size.x;
        child->draw(context, {x, bounds.getBottom() - size.y, size.x, size.y});
        x -= spacing;
    }
}

} // namespace haylen::ui
