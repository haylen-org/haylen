#include "ui/components/settings/SettingsActions.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

// The buttons take the width of one row while they fit in it, and otherwise the width they stack across.
math::Vec2 SettingsActions::measureContent(Context& context, float availableWidth) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    if (isStacked(context, availableWidth)) {
        float height = 0.0F;
        for (Component* child : getLayoutChildren()) {
            height += child->measure(context, availableWidth).y + spacing;
        }
        return {availableWidth, height};
    }

    math::Vec2 size;
    for (Component* child : getLayoutChildren()) {
        const math::Vec2 measured = child->measure(context, CommonProperties::kUnbounded);
        size = {size.x + measured.x + spacing, std::max(size.y, measured.y)};
    }
    return {std::max(0.0F, size.x - spacing), size.y + spacing};
}

// The buttons stand below the spacing that parts them from the rows. In one row they line up to the end edge, which is the left one in a right-to-left UI, and center across the height of the tallest, so a link stands on the middle line of the buttons beside it. When they do not fit in one row they stack across the width, with the last one, usually the main action, on top.
void SettingsActions::render(Context& context, const math::Rect& bounds) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    const float top = bounds.y + spacing;
    if (isStacked(context, bounds.width)) {
        float y = top;
        for (Component* child : getLayoutChildren() | std::views::reverse) {
            const float height = child->measure(context, bounds.width).y;
            child->draw(context, {bounds.x, y, bounds.width, height});
            y += height + spacing;
        }
        return;
    }

    const float height = std::max(0.0F, bounds.height - spacing);
    float x = bounds.getRight();
    for (Component* child : getLayoutChildren() | std::views::reverse) {
        const math::Vec2 size = child->measure(context, CommonProperties::kUnbounded);
        x -= size.x;
        child->draw(context, context.mirror({x, std::floor(top + (height - size.y) * 0.5F), size.x, size.y}, bounds));
        x -= spacing;
    }
}

bool SettingsActions::isStacked(Context& context, float width) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    float row = 0.0F;
    for (Component* child : getLayoutChildren()) {
        row += child->measure(context, CommonProperties::kUnbounded).x + (row > 0.0F ? spacing : 0.0F);
    }
    return row > width;
}

} // namespace haylen::ui
