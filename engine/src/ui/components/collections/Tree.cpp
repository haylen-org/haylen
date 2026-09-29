#include "ui/components/collections/Tree.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/ui/Context.hpp"
#include "ui/Widgets.hpp"
#include "ui/components/ChoiceItem.hpp"
#include "ui/components/collections/ListRow.hpp"

namespace haylen::ui {

void Tree::readProperties(PropertyReader& reader) {
    ChoiceItem::readList(reader, "items", items, true);
    reader.read("selected", selected);
    if (const core::Json* listed = reader.take("expanded")) {
        if (!PropertyReader::isList(*listed) || !std::ranges::all_of(*listed, [](const core::Json& entry) { return entry.is_string(); })) {
            reader.fail("expanded", "must be a list of item ids");
        }
        expanded.clear();
        for (const core::Json& entry : *listed) {
            expanded.insert(entry.get<std::string>());
        }
    }
}

math::Vec2 Tree::measureContent(Context& context, float availableWidth) {
    return {availableWidth, context.getMetric(Theme::Metric::ListRowHeight) * static_cast<float>(countVisible(items))};
}

void Tree::render(Context& context, const math::Rect& bounds) {
    pressedDirection = takeFocusDirection(context);
    focusing = takeFocusRequest();
    float y = bounds.y;
    drawItems(context, bounds, items, 0, y);
}

std::size_t Tree::countVisible(const std::vector<ChoiceItem>& branch) const {
    std::size_t count = branch.size();
    for (const ChoiceItem& item : branch) {
        if (expanded.contains(item.id)) {
            count += countVisible(item.children);
        }
    }
    return count;
}

void Tree::drawItems(Context& context, const math::Rect& bounds, const std::vector<ChoiceItem>& branch, int depth, float& y) {
    const float height = context.getMetric(Theme::Metric::ListRowHeight);
    const float indent = context.getMetric(Theme::Metric::IconSize);
    for (const ChoiceItem& item : branch) {
        const math::Rect area{bounds.x, y, bounds.width, height};
        const float left = bounds.x + indent * static_cast<float>(depth);
        const bool open = expanded.contains(item.id);
        y += height;

        ImGui::PushID(item.id.c_str());
        ImGui::BeginDisabled(!item.enabled);

        // The arrow claims the pointer before the row under it, since ImGui hands hover to the first of two overlapping items, and only the row takes the focus, where left and right close and open the item.
        const math::Rect arrow{left, area.y, indent, height};
        bool toggled = !item.children.empty() && Widgets::interact(context, arrow, context.getMetric(Theme::Metric::ControlRadius) * 0.5F, "##toggle", ImGuiButtonFlags_NoNavFocus).clicked;
        const Widgets::Interaction state = ListRow::draw(context, area, item.id == selected);
        if (focusing && (item.id == selected || selected.empty())) {
            focusing = false;
            Widgets::focusItem(context);
        }
        if (pressedDirection && ImGui::GetItemID() == ImGui::GetFocusID() && !item.children.empty() && open == (pressedDirection == FocusDirection::Left)) {
            toggled = true;
            pressedDirection.reset();
        }
        if (!item.children.empty()) {
            Widgets::arrow(arrow.getCenter(), indent * 0.4F, open ? ImGuiDir_Down : ImGuiDir_Right, context.getColor(Theme::Color::TextMuted));
        }
        ListRow::drawContent(context, math::Rect::fromMinMax({left + indent - ListRow::kPadding, area.y}, area.getMax()), item);
        ImGui::EndDisabled();
        ImGui::PopID();

        if (toggled) {
            toggle(context, item);
        } else if (state.clicked) {
            selected = item.id;
            context.emit(*this, "select", {{"item", item.id}});
        }
        if (open && !toggled) {
            drawItems(context, bounds, item.children, depth + 1, y);
        }
    }
}

void Tree::toggle(Context& context, const ChoiceItem& item) {
    const bool open = !expanded.contains(item.id);
    if (open) {
        expanded.insert(item.id);
    } else {
        expanded.erase(item.id);
    }
    context.emit(*this, "toggle", {{"item", item.id}, {"expanded", open}});
}

} // namespace haylen::ui
