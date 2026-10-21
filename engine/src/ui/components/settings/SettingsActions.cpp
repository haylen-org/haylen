#include "ui/components/settings/SettingsActions.hpp"

#include <algorithm>
#include <cmath>
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

// The buttons line up to the end edge, which is the left one in a right-to-left UI, below the spacing that parts them from the rows, and center across the height of the tallest, so a link stands on the middle line of the buttons beside it.
void SettingsActions::render(Context& context, const math::Rect& bounds) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    const float top = bounds.y + spacing;
    const float height = std::max(0.0F, bounds.height - spacing);
    float x = bounds.getRight();
    for (Component* child : getLayoutChildren() | std::views::reverse) {
        const math::Vec2 size = child->measure(context, bounds.width);
        x -= size.x;
        child->draw(context, context.mirror({x, std::floor(top + (height - size.y) * 0.5F), size.x, size.y}, bounds));
        x -= spacing;
    }
}

} // namespace haylen::ui
