#include "ui/components/choices/RadioGroup.hpp"

#include <algorithm>
#include <string>

#include <imgui.h>

#include "haylen/ui/AccessibilityTree.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/Widgets.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

void RadioGroup::readProperties(PropertyReader& reader) {
    ChoiceItem::readList(reader, "items", items);
    reader.read("selected", selected);
    reader.read("horizontal", horizontal);
}

// The options of a vertical group wrap their labels at the width of the group, and the options of a horizontal group keep their labels on one line side by side.
math::Vec2 RadioGroup::measureContent(Context& context, float availableWidth) {
    math::Vec2 size;
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    for (const ChoiceItem& item : items) {
        const math::Vec2 option = Widgets::measureChoice(context, context.getText(item.text), horizontal ? CommonProperties::kUnbounded : availableWidth);
        size = horizontal ? math::Vec2{size.x + option.x + spacing, std::max(size.y, option.y)} : math::Vec2{std::max(size.x, option.x), size.y + option.y};
    }
    if (horizontal && !items.empty()) {
        size.x -= spacing;
    }
    return size;
}

void RadioGroup::render(Context& context, const math::Rect& bounds) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);

    // The focus goes to the selected option, or to the first option that can be picked.
    const std::string focused = takeFocusRequest() ? getFocusTarget() : std::string();
    math::Vec2 cursor = bounds.getMin();
    for (const ChoiceItem& item : items) {
        const std::string_view text = context.getText(item.text);
        const math::Vec2 size = Widgets::measureChoice(context, text, horizontal ? CommonProperties::kUnbounded : bounds.width);
        const math::Rect option = context.mirror({cursor.x, cursor.y, horizontal ? size.x : bounds.width, size.y}, bounds);
        ImGui::PushID(item.id.c_str());
        ImGui::BeginDisabled(!item.enabled);
        const bool picked = Widgets::radio(context, option, item.id == selected, text, "##option");
        context.getAccessibility().addPart(ImGui::GetItemID(), option, platform::AccessibilityElement::Role::RadioButton, text, (item.id == selected ? platform::AccessibilityElement::Checked : 0) | (item.enabled ? 0 : platform::AccessibilityElement::Disabled));
        if (!focused.empty() && item.id == focused) {
            Widgets::focusItem(context);
        }
        ImGui::EndDisabled();
        ImGui::PopID();
        if (picked && item.id != selected) {
            selected = item.id;
            context.emit(*this, "change", {{"value", item.id}});
        }
        cursor = horizontal ? math::Vec2{cursor.x + size.x + spacing, cursor.y} : math::Vec2{cursor.x, cursor.y + size.y};
    }
}

std::string RadioGroup::getFocusTarget() const {
    if (const int index = ChoiceItem::indexOf(items, selected); index >= 0 && items[static_cast<std::size_t>(index)].enabled) {
        return selected;
    }
    const auto enabled = std::ranges::find_if(items, [](const ChoiceItem& item) { return item.enabled; });
    return enabled != items.end() ? enabled->id : std::string();
}

bool RadioGroup::describe(Context&, platform::AccessibilityElement& element) const {
    element.role = platform::AccessibilityElement::Role::RadioGroup;
    return true;
}

} // namespace haylen::ui
