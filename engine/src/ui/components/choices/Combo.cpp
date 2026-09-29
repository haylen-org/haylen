#include "ui/components/choices/Combo.hpp"

#include <algorithm>
#include <optional>
#include <string>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"
#include "ui/components/ChoiceItem.hpp"
#include "ui/components/PopupList.hpp"

namespace haylen::ui {

void Combo::readProperties(PropertyReader& reader) {
    ChoiceItem::readList(reader, "items", items);
    reader.read("selected", selected);
    reader.read("placeholder", placeholder);
}

math::Vec2 Combo::measureContent(Context& context, float) {
    float widest = Typography::measure(context, Theme::Font::Body, context.getText(placeholder)).x;
    for (const ChoiceItem& item : items) {
        widest = std::max(widest, Typography::measure(context, Theme::Font::Body, context.getText(item.text)).x);
    }
    const float height = context.getMetric(Theme::Metric::ControlHeight);
    return {widest + context.getMetric(Theme::Metric::ControlPaddingX) * 2.0F + height * 0.5F, height};
}

void Combo::render(Context& context, const math::Rect& bounds) {
    const Widgets::Interaction state = Widgets::interact(context, bounds, context.getMetric(Theme::Metric::ControlRadius), "##combo");
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
    const bool open = ImGui::IsPopupOpen("##list");
    Surfaces::draw(context, open ? Theme::Surface::FieldFocused : Theme::Surface::Field, bounds, context.getColor(state.hovered ? Theme::Color::BorderStrong : Theme::Color::Raised), context.getColor(open ? Theme::Color::Focus : Theme::Color::Border));

    const int index = ChoiceItem::indexOf(items, selected);
    const bool empty = index < 0;
    const std::string shown = empty ? context.getText(placeholder) : context.getText(items[static_cast<std::size_t>(index)].text);
    const float padding = context.getMetric(Theme::Metric::ControlPaddingX) * 0.5F;
    const float chevron = bounds.height * 0.5F;
    const math::Rect inner = bounds.inset(Surfaces::getPadding(context, Theme::Surface::Field));
    Typography::drawAligned(context, Theme::Font::Body, context.mirror({inner.x + padding, inner.y, inner.width - padding * 2.0F - chevron, inner.height}, inner), context.getColor(empty ? Theme::Color::TextMuted : Theme::Color::Text), shown, Alignment::Start);
    const math::Vec2 center = context.mirror({inner.getRight() - padding - chevron, inner.y, chevron, inner.height}, inner).getCenter();
    Widgets::arrow(center, chevron * 0.5F, ImGuiDir_Down, context.getColor(Theme::Color::TextMuted));

    if (state.clicked) {
        ImGui::OpenPopup("##list");
    }
    Widgets::placePopup(context, bounds, bounds.width);
    if (const std::optional<std::string> picked = PopupList::draw(context, "##list", items, selected, bounds.width); picked && *picked != selected) {
        selected = *picked;
        context.emit(*this, "change", {{"value", selected}});
    }
}

} // namespace haylen::ui
