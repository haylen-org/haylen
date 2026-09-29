#include "ui/components/inputs/Slider.hpp"

#include <algorithm>
#include <optional>

#include "haylen/ui/Context.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Slider::readProperties(PropertyReader& reader) {
    double lowest = minimum;
    double highest = maximum;
    reader.read("value", value);
    reader.read("min", lowest);
    reader.read("max", highest);
    reader.read("step", step, 0.0);
    reader.read("showValue", showValue);
    reader.read("decimals", decimals, 0, 6);
    if (lowest >= highest) {
        reader.fail("min", "must be smaller than max");
    }
    minimum = lowest;
    maximum = highest;
    value = std::clamp(value, minimum, maximum);
}

math::Vec2 Slider::measureContent(Context& context, float availableWidth) {
    return {std::min(availableWidth, 360.0F), context.getMetric(Theme::Metric::ControlHeight)};
}

void Slider::render(Context& context, const math::Rect& bounds) {
    const float label = showValue ? Typography::measure(context, Theme::Font::Body, Typography::formatNumber(maximum, decimals)).x + context.getMetric(Theme::Metric::ItemSpacing) : 0.0F;
    const math::Rect track = context.mirror({bounds.x, bounds.y, std::max(0.0F, bounds.width - label), bounds.height}, bounds);
    bool changed = Widgets::slider(context, track, value, minimum, maximum, step);
    if (const std::optional<FocusDirection> direction = takeFocusDirection(context)) {
        const double amount = step > 0.0 ? step : (maximum - minimum) / kFocusSteps;
        const double moved = std::clamp(value + amount * Widgets::getStep(context, *direction), minimum, maximum);
        changed = changed || moved != value;
        value = moved;
    }
    if (changed) {
        context.emit(*this, "change", {{"value", value}});
    }
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
    if (showValue) {
        Typography::drawAligned(context, Theme::Font::Body, context.mirror({bounds.getRight() - label, bounds.y, label, bounds.height}, bounds), context.getColor(Theme::Color::TextMuted), Typography::formatNumber(value, decimals), Alignment::End);
    }
}

} // namespace haylen::ui
