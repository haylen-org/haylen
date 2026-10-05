#include "ui/components/containers/Tabs.hpp"

#include <algorithm>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

void Tabs::readProperties(PropertyReader& reader) {
    ChoiceItem::readList(reader, "items", items);
    reader.read("selected", selected);
}

math::Vec2 Tabs::measureContent(Context& context, float availableWidth) {
    float strip = 0.0F;
    for (const ChoiceItem& item : items) {
        strip += getTabWidth(context, item);
    }
    math::Vec2 content;
    if (Component* child = getSelectedChild()) {
        content = child->measure(context, availableWidth);
    }
    return {std::max(strip, content.x), context.getMetric(Theme::Metric::ControlHeight) + context.getMetric(Theme::Metric::ItemSpacing) + content.y};
}

void Tabs::render(Context& context, const math::Rect& bounds) {
    const float height = context.getMetric(Theme::Metric::ControlHeight);
    float x = bounds.x;
    const int chosen = getSelectedIndex();
    Widgets::line(context, {bounds.x, bounds.y + height}, {bounds.getRight(), bounds.y + height}, context.getMetric(Theme::Metric::BorderWidth), context.getColor(Theme::Color::Border));

    for (std::size_t index = 0; index < items.size(); ++index) {
        const ChoiceItem& item = items[index];
        const math::Rect tab = context.mirror({x, bounds.y, getTabWidth(context, item), height}, bounds);
        x += tab.width;
        const bool current = static_cast<int>(index) == chosen;
        ImGui::PushID(static_cast<int>(index));
        ImGui::BeginDisabled(!item.enabled);
        const Widgets::Interaction state = Widgets::interact(context, tab, context.getMetric(Theme::Metric::ControlRadius) * 0.5F, "##tab");
        ImGui::EndDisabled();
        ImGui::PopID();

        const Theme::Surface surface = current ? Theme::Surface::TabSelected : Theme::Surface::Tab;
        if (context.getSurface(surface) != nullptr) {
            Surfaces::draw(context, surface, tab, math::Color::transparent());
        } else if (state.hovered) {
            Surfaces::fill(context, tab, context.getColor(Theme::Color::Hover), context.getMetric(Theme::Metric::ControlRadius) * 0.5F);
        }
        if (current && context.getSurface(surface) == nullptr) {
            const float thickness = context.getMetric(Theme::Metric::FocusWidth);
            Surfaces::fill(context, {tab.x, tab.getBottom() - thickness, tab.width, thickness}, context.getColor(Theme::Color::Accent), 0.0F);
        }
        Typography::drawAligned(context, Theme::Font::Button, tab, context.getColor(current ? Theme::Color::Text : Theme::Color::TextMuted), context.getText(item.text), Alignment::Center);

        if (state.clicked && !current) {
            selected = item.id;
            context.emit(*this, "select", {{"item", item.id}});
        }
    }

    if (Component* child = getSelectedChild()) {
        const float top = bounds.y + height + context.getMetric(Theme::Metric::ItemSpacing);
        child->draw(context, {bounds.x, top, bounds.width, std::max(0.0F, bounds.getBottom() - top)});
    }
}

float Tabs::getTabWidth(Context& context, const ChoiceItem& item) const {
    return Typography::measure(context, Theme::Font::Button, context.getText(item.text)).x + context.getMetric(Theme::Metric::TabPaddingX) * 2.0F;
}

int Tabs::getSelectedIndex() const {
    const int index = ChoiceItem::indexOf(items, selected);
    return index >= 0 || items.empty() ? index : 0;
}

Component* Tabs::getSelectedChild() const {
    const int index = getSelectedIndex();
    return index >= 0 && static_cast<std::size_t>(index) < getChildren().size() ? getChildren()[static_cast<std::size_t>(index)].get() : nullptr;
}

} // namespace haylen::ui
