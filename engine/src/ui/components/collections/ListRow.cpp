#include "ui/components/collections/ListRow.hpp"

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

void ListRow::drawContent(Context& context, const math::Rect& bounds, const ChoiceItem& item) {
    float x = bounds.x + kPadding;
    if (!item.image.empty()) {
        const float icon = context.getMetric(Theme::Metric::IconSize);
        Surfaces::drawImage(context, context.getImage(item.image), {x, std::floor(bounds.getCenter().y - icon * 0.5F), icon, icon});
        x += icon + kPadding;
    }
    const math::Rect text = math::Rect::fromMinMax({x, bounds.y}, {bounds.getRight() - kPadding, bounds.getBottom()});
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

} // namespace haylen::ui
