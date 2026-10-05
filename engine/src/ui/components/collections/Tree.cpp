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

// A tree fills the width it gets, and an unbounded width, such as the one of a horizontal scroll, gets the width of its widest row that shows.
math::Vec2 Tree::measureContent(Context& context, float availableWidth) {
    const float height = context.getMetric(Theme::Metric::ListRowHeight) * static_cast<float>(countVisible(items));
    return {availableWidth < CommonProperties::kUnbounded ? availableWidth : measureWidth(context, items, 0), height};
}

// The focus goes to the selected item when it shows and can take it, or to the first item that can.
void Tree::render(Context& context, const math::Rect& bounds) {
    pressedDirection = takeFocusDirection(context);
    focused.clear();
    if (takeFocusRequest()) {
        const ChoiceItem* target = findShown(items, selected);
        target = target != nullptr ? target : findShown(items, {});
        focused = target != nullptr ? target->id : std::string();
    }
    float y = bounds.y;
    drawItems(context, bounds, items, 0, y);
}

float Tree::measureWidth(Context& context, const std::vector<ChoiceItem>& branch, int depth) const {
    const float indent = context.getMetric(Theme::Metric::IconSize);
    float widest = 0.0F;
    for (const ChoiceItem& item : branch) {
        widest = std::max(widest, indent * static_cast<float>(depth + 1) - context.getMetric(Theme::Metric::RowPadding) + ListRow::measure(context, item));
        if (expanded.contains(item.id)) {
            widest = std::max(widest, measureWidth(context, item.children, depth + 1));
        }
    }
    return widest;
}

const ChoiceItem* Tree::findShown(const std::vector<ChoiceItem>& branch, std::string_view wanted) const {
    for (const ChoiceItem& item : branch) {
        if (item.enabled && (wanted.empty() || item.id == wanted)) {
            return &item;
        }
        if (expanded.contains(item.id)) {
            if (const ChoiceItem* found = findShown(item.children, wanted)) {
                return found;
            }
        }
    }
    return nullptr;
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
        const math::Rect arrow = context.mirror({left, area.y, indent, height}, area);
        bool toggled = !item.children.empty() && Widgets::interact(context, arrow, context.getMetric(Theme::Metric::ControlRadius) * 0.5F, "##toggle", ImGuiButtonFlags_NoNavFocus).clicked;
        const Widgets::Interaction state = ListRow::draw(context, area, item.id == selected);
        if (item.id == focused) {
            Widgets::focusItem(context);
        }
        if (pressedDirection && ImGui::GetItemID() == ImGui::GetFocusID() && !item.children.empty() && open == (Widgets::getStep(context, *pressedDirection) < 0)) {
            toggled = true;
            pressedDirection.reset();
        }
        if (!item.children.empty()) {
            Widgets::arrow(context, arrow.getCenter(), indent * 0.4F, Widgets::mirror(context, open ? ImGuiDir_Down : ImGuiDir_Right), context.getColor(Theme::Color::TextMuted));
        }
        ListRow::drawContent(context, context.mirror(math::Rect::fromMinMax({left + indent - context.getMetric(Theme::Metric::RowPadding), area.y}, area.getMax()), area), item);
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
