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

void Divider::render(Context& context, const math::Rect& bounds) {
    const math::Vec2 center = bounds.getCenter();
    const math::Vec2 from = vertical ? math::Vec2{center.x, bounds.y} : math::Vec2{bounds.x, center.y};
    const math::Vec2 to = vertical ? math::Vec2{center.x, bounds.getBottom()} : math::Vec2{bounds.getRight(), center.y};
    Widgets::line(context, from, to, context.getMetric(Theme::Metric::BorderWidth), context.getColor(color.value_or(Theme::Color::Border)));
}

} // namespace haylen::ui
