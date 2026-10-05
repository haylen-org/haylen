#include "ui/components/inputs/FilterField.hpp"

#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void FilterField::render(Context& context, const math::Rect& bounds) {
    const float icon = bounds.height * 0.6F;
    drawEntry(context, bounds, platform::TextInput::Keyboard::Search, icon, value.empty() ? 0.0F : icon);

    // The magnifying glass is a ring whose stroke runs along the radius of the lens, and a handle out of its lower right side.
    const math::Color muted = context.getColor(Theme::Color::TextMuted);
    const math::Vec2 lens = context.mirror({bounds.x + icon * 0.65F, bounds.getCenter().y - icon * 0.08F, 0.0F, 0.0F}, bounds).getMin();
    const float radius = icon * 0.22F;
    const float stroke = context.getMetric(Theme::Metric::StrokeWidth);
    const float outer = radius + stroke * 0.5F;
    Surfaces::drawShape(context, {.bounds = math::Rect::fromCenter(lens, {outer * 2.0F, outer * 2.0F}), .radii = {outer, outer, outer, outer}, .color = math::Color::transparent(), .borderWidth = stroke, .borderColor = muted});
    Widgets::line(context, lens + math::Vec2{radius * 0.7F, radius * 0.7F}, lens + math::Vec2{radius * 1.6F, radius * 1.6F}, stroke, muted);

    if (value.empty()) {
        return;
    }
    const math::Rect clear = context.mirror({bounds.getRight() - icon, bounds.y, icon, bounds.height}, bounds);
    const Widgets::Interaction state = Widgets::interact(context, clear, context.getMetric(Theme::Metric::ControlRadius), "##clear");
    const math::Vec2 center = clear.getCenter() - math::Vec2{context.isRightToLeft() ? -icon * 0.2F : icon * 0.2F, 0.0F};
    Widgets::cross(context, center, icon * 0.15F, stroke, context.getColor(state.hovered ? Theme::Color::Text : Theme::Color::TextMuted));
    if (state.clicked) {
        value.clear();
        context.emit(*this, "change", {{"value", value}});
    }
}

} // namespace haylen::ui
