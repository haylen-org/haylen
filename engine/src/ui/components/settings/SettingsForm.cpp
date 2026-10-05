#include "ui/components/settings/SettingsForm.hpp"

#include <algorithm>

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

math::Vec2 SettingsForm::measureContent(Context& context, float availableWidth) {
    math::Vec2 size;
    bool first = true;
    for (Component* child : getLayoutChildren()) {
        const math::Vec2 measured = child->measure(context, availableWidth);
        size = {std::max(size.x, measured.x), size.y + measured.y + getSpacingBefore(context, *child, first)};
        first = false;
    }
    return size;
}

void SettingsForm::render(Context& context, const math::Rect& bounds) {
    float y = bounds.y;
    bool first = true;
    for (Component* child : getLayoutChildren()) {
        y += getSpacingBefore(context, *child, first);
        first = false;
        const math::Vec2 size = child->measure(context, bounds.width);
        child->draw(context, {bounds.x, y, bounds.width, size.y});
        y += size.y;
    }
}

float SettingsForm::getSpacingBefore(Context& context, const Component& child, bool first) {
    if (first) {
        return 0.0F;
    }
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    return child.getKind() == "sectionTitle" ? spacing * 2.0F : spacing * 0.5F;
}

} // namespace haylen::ui
