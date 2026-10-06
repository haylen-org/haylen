#include "ui/components/containers/Divider.hpp"

#include "haylen/ui/Context.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Divider::readProperties(PropertyReader& reader) {
    reader.read("vertical", vertical);
    reader.read("color", color);
}

math::Vec2 Divider::measureContent(Context& context, float) {
    const float thickness = context.getMetric(Theme::Metric::BorderWidth);
    return {thickness, thickness};
}

// The line runs through the middle of the bounds across the axis it divides, inside them.
void Divider::render(Context& context, const math::Rect& bounds) {
    const float thickness = context.getMetric(Theme::Metric::BorderWidth);
    const math::Vec2 start = bounds.getCenter() - math::Vec2{thickness * 0.5F, thickness * 0.5F};
    const math::Vec2 from = vertical ? math::Vec2{start.x, bounds.y} : math::Vec2{bounds.x, start.y};
    const math::Vec2 to = vertical ? math::Vec2{start.x, bounds.getBottom()} : math::Vec2{bounds.getRight(), start.y};
    Widgets::line(context, from, to, thickness, context.getColor(color.value_or(Theme::Color::Border)));
}

} // namespace haylen::ui
