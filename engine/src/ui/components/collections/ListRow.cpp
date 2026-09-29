#include "ui/components/collections/ListRow.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

Widgets::Interaction ListRow::draw(Context& context, const math::Rect& bounds, bool selected) {
    const float radius = context.getMetric(Theme::Metric::ControlRadius) * 0.5F;
    const Widgets::Interaction state = Widgets::interact(context, bounds, radius, "##row");
    ImDrawList& list = *ImGui::GetWindowDrawList();
    if (selected) {
        list.AddRectFilled(ImGuiConverter::toImVec2(bounds.getMin()), ImGuiConverter::toImVec2(bounds.getMax()), ImGuiConverter::toImU32(context.getColor(Theme::Color::Selection)), radius);
    } else if (state.hovered) {
        list.AddRectFilled(ImGuiConverter::toImVec2(bounds.getMin()), ImGuiConverter::toImVec2(bounds.getMax()), ImGuiConverter::toImU32(context.getColor(Theme::Color::Hover)), radius);
    }
    return state;
}

// The image stands on the side the UI starts, before the text.
void ListRow::drawContent(Context& context, const math::Rect& bounds, const ChoiceItem& item) {
    if (!Widgets::isVisible(context, bounds)) {
        return;
    }
    float x = bounds.x + kPadding;
    if (!item.image.empty()) {
        const float icon = context.getMetric(Theme::Metric::IconSize);
        Surfaces::drawImage(context, context.getImage(item.image), context.mirror({x, std::floor(bounds.getCenter().y - icon * 0.5F), icon, icon}, bounds));
        x += icon + kPadding;
    }
    const math::Rect text = context.mirror(math::Rect::fromMinMax({x, bounds.y}, {bounds.getRight() - kPadding, bounds.getBottom()}), bounds);
    const std::string caption = context.getText(item.caption);
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
    const float image = item.image.empty() ? 0.0F : context.getMetric(Theme::Metric::IconSize) + kPadding;
    const float text = std::max(Typography::measure(context, Theme::Font::Body, context.getText(item.text)).x, Typography::measure(context, Theme::Font::Caption, context.getText(item.caption)).x);
    return image + text + kPadding * 2.0F;
}

} // namespace haylen::ui
