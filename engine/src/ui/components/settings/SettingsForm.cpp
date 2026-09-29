#include "ui/components/settings/SettingsForm.hpp"

#include <algorithm>
#include <vector>

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

math::Vec2 SettingsForm::measureContent(Context& context, float availableWidth) {
    math::Vec2 size;
    const std::vector<Component*> visible = getLayoutChildren();
    for (std::size_t index = 0; index < visible.size(); ++index) {
        const math::Vec2 child = visible[index]->measure(context, availableWidth);
        size = {std::max(size.x, child.x), size.y + child.y + getSpacingBefore(context, index, visible)};
    }
    return size;
}

void SettingsForm::render(Context& context, const math::Rect& bounds) {
    float y = bounds.y;
    const std::vector<Component*> visible = getLayoutChildren();
    for (std::size_t index = 0; index < visible.size(); ++index) {
        y += getSpacingBefore(context, index, visible);
        const math::Vec2 size = visible[index]->measure(context, bounds.width);
        visible[index]->draw(context, {bounds.x, y, bounds.width, size.y});
        y += size.y;
    }
}

float SettingsForm::getSpacingBefore(Context& context, std::size_t index, const std::vector<Component*>& visible) {
    if (index == 0) {
        return 0.0F;
    }
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    return visible[index]->getKind() == "sectionTitle" ? spacing * 2.0F : spacing * 0.5F;
}

} // namespace haylen::ui
