#include "ui/components/collections/ListRow.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

Widgets::Interaction ListRow::draw(Context& context, const math::Rect& bounds, bool selected, float rounding) {
    const float radius = rounding < 0.0F ? context.getMetric(Theme::Metric::ControlRadius) * 0.5F : rounding;
    const Widgets::Interaction state = Widgets::interact(context, bounds, radius, "##row");
    if (selected || state.held || state.hovered) {
        const Theme::Surface surface = selected ? Theme::Surface::CellSelected : state.held ? Theme::Surface::CellPressed : Theme::Surface::CellHover;
        const Theme::Color color = selected ? Theme::Color::Selection : state.held ? Theme::Color::Pressed : Theme::Color::Hover;
        Surfaces::draw(context, surface, bounds, context.getColor(color), std::nullopt, rounding < 0.0F ? context.getInnerRadii(bounds, radius) : Surfaces::getRadii(radius, ImDrawFlags_RoundCornersAll));
    }
    return state;
}

// The image stands on the side the UI starts, before the text, and takes the color of the text unless the item sets its icon color.
void ListRow::drawContent(Context& context, const math::Rect& bounds, const ChoiceItem& item) {
    if (!Widgets::isVisible(context, bounds)) {
        return;
    }
    float x = bounds.x + context.getMetric(Theme::Metric::RowPadding);
    if (!item.image.empty()) {
        const float icon = context.getMetric(Theme::Metric::IconSize);
        Surfaces::drawImage(context, context.getImage(item.image, {icon, icon}), context.mirror({x, std::floor(bounds.getCenter().y - icon * 0.5F), icon, icon}, bounds), item.iconColor.resolve(context, context.getColor(Theme::Color::Text)));
        x += icon + context.getMetric(Theme::Metric::RowPadding);
    }
    const math::Rect text = context.mirror(math::Rect::fromMinMax({x, bounds.y}, {bounds.getRight() - context.getMetric(Theme::Metric::RowPadding), bounds.getBottom()}), bounds);
    const std::string_view caption = context.getText(item.caption);
    if (caption.empty()) {
        Typography::drawAligned(context, Theme::Font::Body, text, context.getColor(Theme::Color::Text), context.getText(item.text), Alignment::Start);
        return;
    }
    const float body = Typography::getLineHeight(context, Theme::Font::Body);
    const float small = Typography::getLineHeight(context, Theme::Font::Caption);
    const float top = bounds.getCenter().y - (body + small) * 0.5F;
    Typography::drawAligned(context, Theme::Font::Body, {text.x, top, text.width, body}, context.getColor(Theme::Color::Text), context.getText(item.text), Alignment::Start);
    Typography::drawAligned(context, Theme::Font::Caption, {text.x, top + body, text.width, small}, context.getColor(Theme::Color::TextMuted), caption, Alignment::Start);
}

float ListRow::measure(Context& context, const ChoiceItem& item) {
    const float image = item.image.empty() ? 0.0F : context.getMetric(Theme::Metric::IconSize) + context.getMetric(Theme::Metric::RowPadding);
    const float text = std::max(Typography::measure(context, Theme::Font::Body, context.getText(item.text)).x, Typography::measure(context, Theme::Font::Caption, context.getText(item.caption)).x);
    return image + text + context.getMetric(Theme::Metric::RowPadding) * 2.0F;
}

} // namespace haylen::ui
